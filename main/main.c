#include "actuators.h"
#include "display_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "reporter_task.h"
#include "sensor_data_shared.h"
#include "sensor_task.h"
#include "sensors.h"
#include "stats_task.h"

static const char *TAG = "MAIN";

// Task handles (non-static so other files can access them via extern)
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t display_task_handle = NULL;
TaskHandle_t stats_task_handle = NULL;
TaskHandle_t reporter_task_handle = NULL;

volatile int g_latest_light_reading = 0;
volatile int g_latest_water_reading = 0;

/**
 * LED timer callback
 *
 * It toggles both LEDs, creating an alternating blink pattern.
 * The blinking period is either 500ms or 100ms depending on
 * the raw value of water_sensor.
 *
 * IMPORTANT: Timer callbacks must be quick and non-blocking!
 * - Don't use vTaskDelay()
 * - Don't block on queues with long timeouts
 * - Don't do heavy processing
 *
 * If you need blocking operations, use a task instead.
 */
static void led_timer_callback(TimerHandle_t xTimer) {
    // Toggle both LEDs
    // The actuator driver is thread-safe (uses mutex internally)
    led_toggle(LED_YELLOW_ROOF);
    led_toggle(LED_WHITE_GARDEN);

    static TickType_t current_period = pdMS_TO_TICKS(500);
    static TickType_t new_period = pdMS_TO_TICKS(500);

    // Calculate new period
    if (g_latest_water_reading > 30) {
        new_period = pdMS_TO_TICKS(100);
    }
    if (g_latest_water_reading < 15) {
        new_period = pdMS_TO_TICKS(500);
    }

    if (new_period != current_period) {
        xTimerChangePeriod(xTimer, new_period, 0);
        current_period = new_period;
    }
}

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

    // Create mutex for shared sensor data
    ESP_LOGI(TAG, "Creating shared data mutex...");
    g_shared_data_mutex = xSemaphoreCreateMutex();
    if (g_shared_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create shared data mutex");
        return;
    }

    // Create event group for sensor coordination
    ESP_LOGI(TAG, "Creating sensor event group...");
    EventGroupHandle_t sensor_events = xEventGroupCreate();
    if (sensor_events == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

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
    // Stack: 2KB - needs space for sensor driver calls and logging
    ESP_LOGI(TAG, "  Creating sensor_task (priority: 5, stack: 2KB)...");

    static sensor_task_params_t sensor_params;
    sensor_params.queue = sensor_queue;
    sensor_params.events = sensor_events;

    ret = xTaskCreate(sensor_task,         // Task function
                      "sensor",            // Task name (for debugging)
                      2048,                // Stack size in bytes
                      &sensor_params,      // Parameters (queue and event group handles)
                      5,                   // Priority
                      &sensor_task_handle  // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return;
    }

    // Create reporter task
    ESP_LOGI(TAG, "  Creating reporter_task (priority: 4, stack: 4KB)...");
    ret = xTaskCreate(reporter_task, "reporter", 4096,
                      sensor_events,  // Pass event group
                      4, &reporter_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reporter task");
        return;
    }

    // Display task: Receives from queue and prints to console
    // Priority: 4 (lower than sensor) - display is less important than collection
    // Stack: 2KB - moderate for logging
    ESP_LOGI(TAG, "  Creating display_task (priority: 4, stack: 2KB)...");
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

    // Create LED blink timer (instead of led_task)
    ESP_LOGI(TAG, "  Creating led_timer (period: 500ms)...");
    TimerHandle_t led_timer = xTimerCreate("led_blink",         // Timer name (for debugging)
                                           pdMS_TO_TICKS(500),  // Period: 500ms
                                           pdTRUE,  // Auto-reload: timer repeats automatically
                                           NULL,    // Timer ID: not used
                                           led_timer_callback  // Callback function
    );

    if (led_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED timer");
        return;
    }

    // Start the timer
    // Second parameter is block time: 0 means don't wait if timer queue is full
    if (xTimerStart(led_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start LED timer");
        return;
    }

    // Stats task: Monitors system stats periodically
    // Priority: 2 (lowest) - non-critical monitoring
    // Stack: 2KB - needs space for stats gathering and logging
    ESP_LOGI(TAG, "  Creating stats_task (priority: 2, stack: 2KB)...");
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
    // - led_timer: Blinking LEDs every 500ms or 100ms depending on the water_sensor raw value
}
