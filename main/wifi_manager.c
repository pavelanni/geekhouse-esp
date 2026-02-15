#include "wifi_manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "wifi_config.h"

static const char *TAG = "WIFI_MGR";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;

/**
 * WiFi and IP event handler
 *
 * This function is called by the ESP-IDF event loop when WiFi
 * or IP events occur. It runs in the event task context.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // WiFi driver started - initiate connection
                ESP_LOGI(TAG, "WiFi started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                // Associated with AP, waiting for IP
                ESP_LOGI(TAG, "Connected to AP, waiting for IP...");
                s_retry_count = 0;
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                // Lost connection - attempt reconnect
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;
                ESP_LOGW(TAG, "Disconnected (reason: %d)", event->reason);

                // Signal disconnected
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);

                if (s_retry_count < WIFI_MAX_RETRIES) {
                    s_retry_count++;
                    ESP_LOGI(TAG, "Reconnecting (attempt %d/%d)...", s_retry_count,
                             WIFI_MAX_RETRIES);
                    esp_wifi_connect();
                } else {
                    ESP_LOGE(TAG, "Max retries reached, giving up");
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // Got IP address - fully connected!
            ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
            ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
            ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));

            // Signal connected
            xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t wifi_manager_init(void) {
    esp_err_t ret;

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station network interface
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    // Load credentials from NVS
    char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
    char password[WIFI_PASSWORD_MAX_LEN + 1] = {0};

    ret = wifi_config_get_ssid(ssid, sizeof(ssid));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SSID from NVS");
        return ret;
    }

    ret = wifi_config_get_password(password, sizeof(password));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get password from NVS");
        return ret;
    }

    // Configure WiFi station
    wifi_config_t wifi_cfg = {
        .sta =
            {
                // SSID and password will be copied below
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };

    // Copy credentials (strncpy for safety)
    strncpy((char *) wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *) wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    // Start WiFi (triggers WIFI_EVENT_STA_START → connect)
    ESP_LOGI(TAG, "Starting WiFi (SSID: %s)...", ssid);
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

EventGroupHandle_t wifi_manager_get_event_group(void) {
    return s_wifi_event_group;
}

bool wifi_manager_is_connected(void) {
    if (s_wifi_event_group == NULL) {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
