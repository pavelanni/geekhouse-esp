#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event group bits for WiFi status
#define WIFI_CONNECTED_BIT    BIT0  // Connected and got IP
#define WIFI_DISCONNECTED_BIT BIT1  // Disconnected or failed

// Maximum connection retries before giving up
#define WIFI_MAX_RETRIES 10

/**
 * Initialize and start WiFi in station mode
 *
 * Reads credentials from NVS (wifi_config module), initializes
 * the WiFi driver, and begins connection attempts.
 *
 * Must be called after wifi_config_init().
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * Get WiFi event group
 *
 * Use this to wait for connection status in other tasks:
 *
 *   EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
 *   xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT,
 *                       pdFALSE, pdTRUE, portMAX_DELAY);
 *
 * @return Event group handle
 */
EventGroupHandle_t wifi_manager_get_event_group(void);

/**
 * Check if WiFi is currently connected
 *
 * @return true if connected and has IP address
 */
bool wifi_manager_is_connected(void);

#endif  // WIFI_MANAGER_H
