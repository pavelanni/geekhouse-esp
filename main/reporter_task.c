#include "reporter_task.h"

#include "esp_log.h"
#include "sensor_data_shared.h"

static const char *TAG = "REPORTER";

#define HISTORY_SIZE 10

// Statistics structure
typedef struct {
    int light_min;
    int light_max;
    float light_sum;
    int water_min;
    int water_max;
    float water_sum;
    int count;
} sensor_stats_t;

void reporter_task(void *pvParameters) {
    EventGroupHandle_t events = (EventGroupHandle_t) pvParameters;
    sensor_stats_t stats = {0};

    // Initialize min values to max possible
    stats.light_min = 4095;
    stats.water_min = 4095;

    ESP_LOGI(TAG, "Reporter task started");
    ESP_LOGI(TAG, "Waiting for sensor readings...");

    while (1) {
        // Wait for BOTH sensors to have new data
        // Parameters:
        //   - events: Event group handle
        //   - ALL_SENSORS_READY_BITS: Bits to wait for
        //   - pdTRUE: Clear bits on exit (so we wait for next reading)
        //   - pdTRUE: Wait for ALL bits (AND logic, not OR)
        //   - pdMS_TO_TICKS(5000) means we wait for 5 seconds to read sensors
        EventBits_t bits = xEventGroupWaitBits(events, ALL_SENSORS_READY_BITS,
                                               pdTRUE,              // Clear bits on exit
                                               pdTRUE,              // Wait for all bits (AND)
                                               pdMS_TO_TICKS(5000)  // Wait for 5 sec
        );

        if ((bits & ALL_SENSORS_READY_BITS) == ALL_SENSORS_READY_BITS) {
            // Read from shared structure
            if (xSemaphoreTake(g_shared_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                int light = g_shared_sensor_data.light_raw;
                int water = g_shared_sensor_data.water_raw;
                xSemaphoreGive(g_shared_data_mutex);

                // Update statistics
                if (light < stats.light_min) {
                    stats.light_min = light;
                }
                if (light > stats.light_max) {
                    stats.light_max = light;
                }
                stats.light_sum += light;

                if (water < stats.water_min) {
                    stats.water_min = water;
                }
                if (water > stats.water_max) {
                    stats.water_max = water;
                }
                stats.water_sum += water;

                stats.count++;
            } else {
                ESP_LOGW(TAG, "Failed to acquire mutex");
            }
        } else {
            // Timeout! Check which sensor is missing
            if (!(bits & LIGHT_SENSOR_READY_BIT)) {
                ESP_LOGW(TAG, "Light sensor timed out!");
            }
            if (!(bits & WATER_SENSOR_READY_BIT)) {
                ESP_LOGW(TAG, "Water sensor timed out!");
            }
        }

        // Print summary every 10 readings
        if (stats.count >= HISTORY_SIZE) {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "===== Sensor Summary (last %d readings) =====", HISTORY_SIZE);
            ESP_LOGI(TAG, "  Light: min=%d, max=%d, avg=%.0f", stats.light_min, stats.light_max,
                     stats.light_sum / HISTORY_SIZE);
            ESP_LOGI(TAG, "  Water: min=%d, max=%d, avg=%.0f", stats.water_min, stats.water_max,
                     stats.water_sum / HISTORY_SIZE);
            ESP_LOGI(TAG, "==========================================");
            ESP_LOGI(TAG, "");

            // Reset statistics
            stats.light_min = 4095;
            stats.light_max = 0;
            stats.light_sum = 0;
            stats.water_min = 4095;
            stats.water_max = 0;
            stats.water_sum = 0;
            stats.count = 0;
        }
    }
}
