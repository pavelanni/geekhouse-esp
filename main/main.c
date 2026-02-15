#include "actuators.h"
#include "display_task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "network_task.h"
#include "nvs_flash.h"
#include "reporter_task.h"
#include "sensor_data_shared.h"
#include "sensor_task.h"
#include "sensors.h"
#include "stats_task.h"
#include "wifi_config.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

#define SENSOR_TASK_STACK      2048
#define SENSOR_TASK_PRIORITY   5
#define REPORTER_TASK_STACK    2048
#define REPORTER_TASK_PRIORITY 4
#define DISPLAY_TASK_STACK     2048
#define DISPLAY_TASK_PRIORITY  4
#define STATS_TASK_STACK       2048
#define STATS_TASK_PRIORITY    2
#define NETWORK_TASK_STACK     4096
#define NETWORK_TASK_PRIORITY  2

// Task handles (non-static so other files can access them via extern)
TaskHandle_t sensor_task_handle = NULL;
TaskHandle_t display_task_handle = NULL;
TaskHandle_t stats_task_handle = NULL;
TaskHandle_t reporter_task_handle = NULL;
TaskHandle_t network_task_handle = NULL;

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Geekhouse FreeRTOS version ===");
    ESP_LOGI(TAG, "");

    // Initialize NVS (must be before wifi_config_init)
    ESP_LOGI(TAG, "Initializing NVS flash...");
    esp_err_t ret_nvs = nvs_flash_init();
    if (ret_nvs == ESP_ERR_NVS_NO_FREE_PAGES || ret_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was corrupted or wrong version - erase and retry
        ESP_LOGW(TAG, "NVS corrupted, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret_nvs);

    // Initialize WiFi configuration from NVS
    ESP_LOGI(TAG, "Initializing WiFi configuration...");
    ESP_ERROR_CHECK(wifi_config_init());

    // Print current credentials
    char ssid[WIFI_SSID_MAX_LEN + 1];
    wifi_config_get_ssid(ssid, sizeof(ssid));
    ESP_LOGI(TAG, "Configured WiFi SSID: %s", ssid);

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

    BaseType_t ret = pdPASS;

    // Sensor task: Reads ADC periodically and pushes to queue
    // Priority: 5 (medium) - important but not time-critical
    // Stack: 2KB - needs space for sensor driver calls and logging
    ESP_LOGI(TAG, "  Creating sensor_task (priority: 5, stack: 2KB)...");

    static sensor_task_params_t sensor_params;
    sensor_params.queue = sensor_queue;
    sensor_params.events = sensor_events;

    ret = xTaskCreate(sensor_task,           // Task function
                      "sensor",              // Task name (for debugging)
                      SENSOR_TASK_STACK,     // Stack size in bytes
                      &sensor_params,        // Parameters (queue and event group handles)
                      SENSOR_TASK_PRIORITY,  // Priority
                      &sensor_task_handle    // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return;
    }

    // Create reporter task
    ESP_LOGI(TAG, "  Creating reporter_task (priority: 4, stack: 2KB)...");
    ret = xTaskCreate(reporter_task, "reporter", REPORTER_TASK_STACK,
                      sensor_events,  // Pass event group
                      REPORTER_TASK_PRIORITY, &reporter_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reporter task");
        return;
    }

    // Display task: Receives from queue and prints to console
    // Priority: 4 (lower than sensor) - display is less important than collection
    // Stack: 2KB - moderate for logging
    ESP_LOGI(TAG, "  Creating display_task (priority: 4, stack: 2KB)...");
    ret = xTaskCreate(display_task,           // Task function
                      "display",              // Task name (for debugging)
                      DISPLAY_TASK_STACK,     // Stack size in bytes
                      sensor_queue,           // Parameter (same queue)
                      DISPLAY_TASK_PRIORITY,  // Priority
                      &display_task_handle    // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return;
    }

    esp_err_t ret_led = led_blink_start();
    if (ret_led != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LED blinking task");
    }

    // Stats task: Monitors system stats periodically
    // Priority: 2 (lowest) - non-critical monitoring
    // Stack: 2KB - needs space for stats gathering and logging
    ESP_LOGI(TAG, "  Creating stats_task (priority: 2, stack: 2KB)...");
    ret = xTaskCreate(stats_task,           // Task function
                      "stats",              // Task name
                      STATS_TASK_STACK,     // Stack size (needs space for buffers)
                      NULL,                 // No parameters
                      STATS_TASK_PRIORITY,  // Priority (lower than all functional tasks)
                      &stats_task_handle    // Task handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stats task");
        return;
    }
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(wifi_manager_init());

    // Network task: wait for WiFi to be ready and start HTTP server
    ESP_LOGI(TAG, "Starting network task...");
    ret = xTaskCreate(network_task, "network", NETWORK_TASK_STACK, NULL, NETWORK_TASK_PRIORITY,
                      &network_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
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
}
