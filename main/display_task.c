#include "display_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sensors.h"

static const char *TAG = "DISPLAY_TASK";

void display_task(void *pvParameters) {
    // Cast parameter to queue handle
    // This is the same queue that sensor_task is pushing to
    QueueHandle_t queue = (QueueHandle_t) pvParameters;
    sensor_reading_t reading;

    ESP_LOGI(TAG, "Display task started");
    ESP_LOGI(TAG, "Waiting for sensor readings...");

    // Task loop - runs forever as a consumer
    while (1) {
        // Block indefinitely waiting for queue item
        // portMAX_DELAY means wait forever (no timeout)
        // This is efficient - task is suspended until data arrives
        if (xQueueReceive(queue, &reading, portMAX_DELAY) == pdTRUE) {
            // We got a reading from the queue!
            // Get sensor metadata to print nice output
            const sensor_info_t *info = sensor_get_info(reading.id);

            if (info != NULL) {
                // Print sensor type name
                const char *type_name = (info->type == SENSOR_TYPE_LIGHT) ? "Light" : "Water";

                // Print formatted reading with all information
                ESP_LOGI(TAG, "%s sensor (%s): raw=%d, calibrated=%.2f %s, time=%lu ms", type_name,
                         info->location, reading.raw_value, reading.calibrated_value, reading.unit,
                         reading.timestamp);
            } else {
                // This shouldn't happen, but handle gracefully
                ESP_LOGW(TAG, "Unknown sensor ID: %d", reading.id);
            }
        }
        // If xQueueReceive fails (should never happen with portMAX_DELAY),
        // we'll just loop around and try again
    }

    // Note: This task never exits. If it did, we'd need vTaskDelete(NULL) here.
}
