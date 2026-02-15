#include "network_task.h"

#include "esp_log.h"
#include "http_server.h"
#include "wifi_manager.h"

static const char *TAG = "NETWORK_TASK";

void network_task(void *pvParameters) {
    // Wait for WiFi connection before starting network services
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                                           pdFALSE,              // Don't clear bits
                                           pdFALSE,              // Wait for ANY bit (OR)
                                           pdMS_TO_TICKS(30000)  // 30 second timeout
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected!");

        // Start HTTP server
        ESP_LOGI(TAG, "Starting HTTP server...");
        ESP_ERROR_CHECK(http_server_start());
        ESP_LOGI(TAG, "Network task done, deleting self");

    } else {
        ESP_LOGW(TAG, "WiFi connection timed out, HTTP server not started");
        ESP_LOGI(TAG, "Network task failed, deleting self");
    }
    vTaskDelete(NULL);
}