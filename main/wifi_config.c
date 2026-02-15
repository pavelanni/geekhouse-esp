#include "wifi_config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "WIFI_CONFIG";

// NVS namespace and keys
#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "password"

static nvs_handle_t s_nvs_handle;

esp_err_t wifi_config_init(void) {
    esp_err_t ret = ESP_OK;

    // Open NVS namespace
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // Check if SSID exists in NVS
    size_t required_size = 0;
    ret = nvs_get_str(s_nvs_handle, NVS_KEY_SSID, NULL, &required_size);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // First boot: store Kconfig defaults into NVS
        ESP_LOGI(TAG, "No WiFi credentials in NVS, storing defaults from Kconfig");

        ret = nvs_set_str(s_nvs_handle, NVS_KEY_SSID, CONFIG_GEEKHOUSE_WIFI_SSID);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to store default SSID: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_set_str(s_nvs_handle, NVS_KEY_PASS, CONFIG_GEEKHOUSE_WIFI_PASSWORD);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to store default password: %s", esp_err_to_name(ret));
            return ret;
        }

        // Commit writes to flash
        ret = nvs_commit(s_nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "Default credentials stored (SSID: %s)", CONFIG_GEEKHOUSE_WIFI_SSID);
    } else if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials found in NVS");
    } else {
        ESP_LOGE(TAG, "Error reading NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_config_get_ssid(char *ssid, size_t len) {
    return nvs_get_str(s_nvs_handle, NVS_KEY_SSID, ssid, &len);
}

esp_err_t wifi_config_get_password(char *password, size_t len) {
    return nvs_get_str(s_nvs_handle, NVS_KEY_PASS, password, &len);
}

esp_err_t wifi_config_set_credentials(const char *ssid, const char *password) {
    esp_err_t ret = ESP_OK;

    ret = nvs_set_str(s_nvs_handle, NVS_KEY_SSID, ssid);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(s_nvs_handle, NVS_KEY_PASS, password);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Credentials updated (SSID: %s)", ssid);
    return ESP_OK;
}
