#include "http_server.h"

#include <string.h>

#include "actuators.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sensors.h"

static const char *TAG = "HTTP_SRV";
static httpd_handle_t s_server = NULL;

/**
 * Helper: Send JSON response
 *
 * Sets Content-Type header and sends the cJSON object as response.
 * Frees the cJSON object after sending.
 */
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json) {
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (json_str == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to generate JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

/**
 * Helper: Send error JSON response
 */
static esp_err_t send_error_response(httpd_req_t *req, int status, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message);

    httpd_resp_set_status(req, status == 404 ? "404 Not Found" : "400 Bad Request");
    return send_json_response(req, json);
}

// ---- GET /api/sensors ----

static esp_err_t get_sensors_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *sensors = cJSON_AddArrayToObject(root, "sensors");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        const sensor_info_t *info = sensor_get_info(i);
        sensor_reading_t reading;
        esp_err_t ret = sensor_read(i, &reading);

        cJSON *sensor = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensor, "id", i);
        cJSON_AddStringToObject(sensor, "type",
                                info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
        cJSON_AddStringToObject(sensor, "location", info->location);

        if (ret == ESP_OK) {
            cJSON_AddNumberToObject(sensor, "raw_value", reading.raw_value);
            cJSON_AddNumberToObject(sensor, "calibrated_value", reading.calibrated_value);
            cJSON_AddStringToObject(sensor, "unit", reading.unit);
            cJSON_AddNumberToObject(sensor, "timestamp", reading.timestamp);
        } else {
            cJSON_AddStringToObject(sensor, "error", "read failed");
        }

        cJSON_AddItemToArray(sensors, sensor);
    }

    return send_json_response(req, root);
}

// ---- GET /api/sensors/{id} ----

static esp_err_t get_sensor_by_id_handler(httpd_req_t *req) {
    // Extract sensor ID from URI
    // URI is like "/api/sensors/0" - get the last character
    const char *uri = req->uri;
    int id = uri[strlen("/api/sensors/")] - '0';

    if (id < 0 || id >= SENSOR_COUNT) {
        return send_error_response(req, 404, "Sensor not found");
    }

    const sensor_info_t *info = sensor_get_info(id);
    sensor_reading_t reading;
    esp_err_t ret = sensor_read(id, &reading);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "type", info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
    cJSON_AddStringToObject(root, "location", info->location);

    if (ret == ESP_OK) {
        cJSON_AddNumberToObject(root, "raw_value", reading.raw_value);
        cJSON_AddNumberToObject(root, "calibrated_value", reading.calibrated_value);
        cJSON_AddStringToObject(root, "unit", reading.unit);
        cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
    }

    return send_json_response(req, root);
}

// ---- GET /api/leds ----

static esp_err_t get_leds_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *leds = cJSON_AddArrayToObject(root, "leds");

    for (int i = 0; i < LED_COUNT; i++) {
        const led_info_t *info = led_get_info(i);
        bool state = false;
        led_get_state(i, &state);

        cJSON *led = cJSON_CreateObject();
        cJSON_AddNumberToObject(led, "id", i);
        cJSON_AddStringToObject(led, "color", info->color);
        cJSON_AddStringToObject(led, "location", info->location);
        cJSON_AddBoolToObject(led, "state", (cJSON_bool) state);

        cJSON_AddItemToArray(leds, led);
    }

    return send_json_response(req, root);
}

// ---- POST /api/leds/{id} ----
// Body: {"action": "on"} or {"action": "off"} or {"action": "toggle"}

static esp_err_t post_led_handler(httpd_req_t *req) {
    // Extract LED ID from URI
    const char *uri = req->uri;
    int id = uri[strlen("/api/leds/")] - '0';

    if (id < 0 || id >= LED_COUNT) {
        return send_error_response(req, 404, "LED not found");
    }

    // Read request body
    char body[128] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return send_error_response(req, 400, "Empty request body");
    }

    // Parse JSON body
    cJSON *json = cJSON_Parse(body);
    if (json == NULL) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    cJSON *action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action)) {
        cJSON_Delete(json);
        return send_error_response(req, 400, "Missing 'action' field (on/off/toggle)");
    }

    // Execute action
    esp_err_t ret = ESP_OK;
    if (strcmp(action->valuestring, "on") == 0) {
        ret = led_on(id);
    } else if (strcmp(action->valuestring, "off") == 0) {
        ret = led_off(id);
    } else if (strcmp(action->valuestring, "toggle") == 0) {
        ret = led_toggle(id);
    } else {
        cJSON_Delete(json);
        return send_error_response(req, 400, "Invalid action (use: on, off, toggle)");
    }

    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return send_error_response(req, 400, "LED operation failed");
    }

    // Return updated LED state
    bool state = false;
    led_get_state(id, &state);
    const led_info_t *info = led_get_info(id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "color", info->color);
    cJSON_AddStringToObject(root, "location", info->location);
    cJSON_AddBoolToObject(root, "state", (cJSON_bool) state);

    return send_json_response(req, root);
}

// ---- GET /api/system ----

static esp_err_t get_system_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    // Uptime
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", (double) uptime_ms);

    // Memory
    cJSON *memory = cJSON_AddObjectToObject(root, "memory");
    cJSON_AddNumberToObject(memory, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(memory, "min_free_heap", esp_get_minimum_free_heap_size());

    // WiFi (basic info)
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
        cJSON_AddStringToObject(wifi, "ssid", (char *) ap_info.ssid);
        cJSON_AddNumberToObject(wifi, "rssi", ap_info.rssi);
        cJSON_AddNumberToObject(wifi, "channel", ap_info.primary);
    }

    return send_json_response(req, root);
}

// ---- URI registration ----

esp_err_t http_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    const httpd_uri_t uris[] = {
        {
            .uri = "/api/sensors",
            .method = HTTP_GET,
            .handler = get_sensors_handler,
        },
        {
            .uri = "/api/sensors/*",
            .method = HTTP_GET,
            .handler = get_sensor_by_id_handler,
        },
        {
            .uri = "/api/leds",
            .method = HTTP_GET,
            .handler = get_leds_handler,
        },
        {
            .uri = "/api/leds/*",
            .method = HTTP_POST,
            .handler = post_led_handler,
        },
        {
            .uri = "/api/system",
            .method = HTTP_GET,
            .handler = get_system_handler,
        },
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started with %d endpoints", (int) (sizeof(uris) / sizeof(uris[0])));
    return ESP_OK;
}

esp_err_t http_server_stop(void) {
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}
