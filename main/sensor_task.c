#include "sensor_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sensors.h"

static const char *TAG = "SENSOR_TASK";

void sensor_task(void *pvParameters) {
    // Cast parameter to queue handle
    // The queue was created in app_main() and passed to us
    QueueHandle_t queue = (QueueHandle_t) pvParameters;
    sensor_reading_t reading;

    ESP_LOGI(TAG, "Sensor task started");
    ESP_LOGI(TAG, "Reading sensors every 2 seconds...");

    // Task loop - runs forever
    // FreeRTOS will preempt us when other tasks need CPU
    while (1) {
        // Read light sensor
        if (sensor_read(SENSOR_LIGHT_ROOF, &reading) == ESP_OK) {
            // Try to send to queue with 100ms timeout
            // If queue is full, this will block for up to 100ms
            if (xQueueSend(queue, &reading, pdMS_TO_TICKS(100)) != pdTRUE) {
                // Queue is full - log warning and drop reading
                ESP_LOGW(TAG, "Queue full, dropping light reading");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read light sensor");
        }

        // Read water sensor
        if (sensor_read(SENSOR_WATER_ROOF, &reading) == ESP_OK) {
            // Try to send to queue with 100ms timeout
            if (xQueueSend(queue, &reading, pdMS_TO_TICKS(100)) != pdTRUE) {
                // Queue is full - log warning and drop reading
                ESP_LOGW(TAG, "Queue full, dropping water reading");
            }
        } else {
            ESP_LOGE(TAG, "Failed to read water sensor");
        }

        // Wait 2 seconds before next reading
        // vTaskDelay() puts this task to sleep, allowing other tasks to run
        // The FreeRTOS scheduler will wake us up after 2 seconds
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Note: This task never exits. If it did, we'd need vTaskDelete(NULL) here.
}
