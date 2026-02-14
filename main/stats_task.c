
#include "stats_task.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "STATS_TASK";

// Threshold for warning (bytes)
#define STACK_WARNING_THRESHOLD 512

// Buffer sizes for task statistics
#define STATS_BUFFER_SIZE 1024

// External task handles (defined in main.c)
extern TaskHandle_t sensor_task_handle;
extern TaskHandle_t display_task_handle;
extern TaskHandle_t stats_task_handle;
extern TaskHandle_t reporter_task_handle;

// Forward declaration of helper function
static void check_task_stack(TaskHandle_t handle, const char *name);

void stats_task(void *pvParameters) {
    (void) pvParameters;

    // Allocate buffers for statistics strings
    // These are large, so allocate from heap rather than stack
    char *task_list_buffer = malloc(STATS_BUFFER_SIZE);
    char *cpu_stats_buffer = malloc(STATS_BUFFER_SIZE);

    if (task_list_buffer == NULL || cpu_stats_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate statistics buffers");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Statistics task started");
    ESP_LOGI(TAG, "Printing task stats every 10 seconds...");
    ESP_LOGI(TAG, "");

    while (1) {
        // Wait 10 seconds before printing stats
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========== TASK STATISTICS ==========");
        ESP_LOGI(TAG, "");

        // Get task state information
        // Format: Name  State  Priority  Stack  TaskNum
        ESP_LOGI(TAG, "Task States (X=Running, B=Blocked, R=Ready, S=Suspended, D=Deleted):");
        vTaskList(task_list_buffer);
        ESP_LOGI(TAG, "\nName            State  Prio     Stack   Num\n%s", task_list_buffer);

        ESP_LOGI(TAG, "");

        // Check individual task stacks with warnings
        ESP_LOGI(TAG, "Stack Analysis:");
        check_task_stack(sensor_task_handle, "sensor");
        check_task_stack(display_task_handle, "display");
        check_task_stack(stats_task_handle, "stats");
        check_task_stack(reporter_task_handle, "reporter");

        ESP_LOGI(TAG, "");

        // Get CPU usage statistics
        // Format: Name  AbsTime  %%Time
        ESP_LOGI(TAG, "CPU Usage:");
        vTaskGetRunTimeStats(cpu_stats_buffer);
        ESP_LOGI(TAG, "\n%s", cpu_stats_buffer);

        ESP_LOGI(TAG, "");

        // Print free heap size
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Minimum free heap (since boot): %lu bytes",
                 esp_get_minimum_free_heap_size());

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=====================================");
        ESP_LOGI(TAG, "");
    }

    // Cleanup (never reached, but good practice)
    free(task_list_buffer);
    free(cpu_stats_buffer);
    vTaskDelete(NULL);
}

// Helper function to check and report stack usage
static void check_task_stack(TaskHandle_t handle, const char *name) {
    if (handle == NULL) {
        ESP_LOGW(TAG, "  %s: handle is NULL", name);
        return;
    }

    // Get high water mark (minimum free stack in bytes)
    UBaseType_t free_stack = uxTaskGetStackHighWaterMark(handle);

    // Log with appropriate level
    if (free_stack < STACK_WARNING_THRESHOLD) {
        ESP_LOGW(TAG, "  ⚠️  %s: only %u bytes free!", name, free_stack);
    } else {
        ESP_LOGI(TAG, "  %s: %u bytes free", name, free_stack);
    }
}