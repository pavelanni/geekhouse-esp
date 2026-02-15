#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_err.h"

// Maximum lengths (WiFi spec limits)
#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASSWORD_MAX_LEN 64

/**
 * Initialize WiFi configuration
 *
 * Opens NVS namespace "wifi_config". If no credentials are stored,
 * writes the Kconfig defaults (CONFIG_GEEKHOUSE_WIFI_SSID/PASSWORD).
 *
 * Must be called after nvs_flash_init().
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_config_init(void);

/**
 * Get stored WiFi SSID
 *
 * @param[out] ssid Buffer to receive SSID (must be >= WIFI_SSID_MAX_LEN + 1)
 * @param len Buffer length
 * @return ESP_OK on success
 */
esp_err_t wifi_config_get_ssid(char *ssid, size_t len);

/**
 * Get stored WiFi password
 *
 * @param[out] password Buffer to receive password (must be >= WIFI_PASSWORD_MAX_LEN + 1)
 * @param len Buffer length
 * @return ESP_OK on success
 */
esp_err_t wifi_config_get_password(char *password, size_t len);

/**
 * Update WiFi credentials in NVS
 *
 * New values persist across reboots. Takes effect on next WiFi connection.
 *
 * @param ssid New SSID
 * @param password New password
 * @return ESP_OK on success
 */
esp_err_t wifi_config_set_credentials(const char *ssid, const char *password);

#endif  // WIFI_CONFIG_H
