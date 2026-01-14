#include "actuators.h"
#include "display_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_task.h"
#include "sensor_task.h"
#include "sensors.h"
#include "stats_task.h"

static const char *TAG = "MAIN";

// Task handles (non-static so other files can access them via extern)
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t display_task_handle = NULL;
TaskHandle_t led_task_handle = NULL;
TaskHandle_t stats_task_handle = NULL;

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Geekhouse Phase 2: Multi-Task Architecture ===");
    ESP_LOGI(TAG, "");

    // ===== Initialize Drivers =====
    ESP_LOGI(TAG, "Initializing drivers...");
    ESP_ERROR_CHECK(led_init());
    ESP_ERROR_CHECK(sensor_init());
    ESP_LOGI(TAG, "Drivers initialized successfully");
    ESP_LOGI(TAG, "");

    // ===== Create Queue =====
    // Queue for passing sensor readings from sensor_task to display_task
    // Capacity: 10 items (enough for 5 seconds of sensor readings at 2s interval)
    // Item size: sizeof(sensor_reading_t)
    ESP_LOGI(TAG, "Creating sensor data queue (capacity: 10)...");
    QueueHandle_t sensor_queue = xQueueCreate(10, sizeof(sensor_reading_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue - out of memory?");
        return;  // Fatal error - can't continue
    }
    ESP_LOGI(TAG, "Queue created successfully");
    ESP_LOGI(TAG, "");

    // ===== Create Tasks =====
    ESP_LOGI(TAG, "Creating FreeRTOS tasks...");

    BaseType_t ret;

    // Sensor task: Reads ADC periodically and pushes to queue
    // Priority: 5 (medium) - important but not time-critical
    // Stack: 4KB - needs space for sensor driver calls and logging
    ESP_LOGI(TAG, "  Creating sensor_task (priority: 5, stack: 4KB)...");
    ret = xTaskCreate(sensor_task,         // Task function
                      "sensor",            // Task name (for debugging)
                      2048,                // Stack size in bytes
                      sensor_queue,        // Parameter (queue handle)
                      5,                   // Priority
                      &sensor_task_handle  // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return;
    }

    // Display task: Receives from queue and prints to console
    // Priority: 4 (lower than sensor) - display is less important than collection
    // Stack: 3KB - moderate for logging
    ESP_LOGI(TAG, "  Creating display_task (priority: 4, stack: 3KB)...");
    ret = xTaskCreate(display_task,         // Task function
                      "display",            // Task name (for debugging)
                      2048,                 // Stack size in bytes
                      sensor_queue,         // Parameter (same queue)
                      4,                    // Priority
                      &display_task_handle  // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return;
    }

    // LED task: Blinks LEDs independently
    // Priority: 3 (low) - just a visual indicator
    // Stack: 2KB - minimal (no complex operations)
    ESP_LOGI(TAG, "  Creating led_task (priority: 3, stack: 2KB)...");
    ret = xTaskCreate(led_task,         // Task function
                      "led",            // Task name (for debugging)
                      1536,             // Stack size in bytes
                      NULL,             // No parameters needed
                      3,                // Priority
                      &led_task_handle  // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        return;
    }

    ESP_LOGI(TAG, "  Creating stats_task (priority: 2, stack: 4KB)...");
    ret = xTaskCreate(stats_task,         // Task function
                      "stats",            // Task name
                      2048,               // Stack size (needs space for buffers)
                      NULL,               // No parameters
                      2,                  // Priority (lower than all functional tasks)
                      &stats_task_handle  // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stats task");
        return;
    }

    ESP_LOGI(TAG, "All tasks created successfully");
    ESP_LOGI(TAG, "");

    // ===== System Running =====
    ESP_LOGI(TAG, "FreeRTOS scheduler is now running");
    ESP_LOGI(TAG, "Tasks are executing independently...");
    ESP_LOGI(TAG, "");

    // app_main() returns here, but the system keeps running!
    // The FreeRTOS scheduler continues executing all created tasks.
    // The app_main task will be automatically deleted by the IDLE task,
    // and its stack memory will be reclaimed.
    //
    // From this point on, the three tasks we created will run concurrently:
    // - sensor_task: Reading sensors every 2s
    // - display_task: Printing readings as they arrive
    // - led_task: Blinking LEDs every 500ms
}
