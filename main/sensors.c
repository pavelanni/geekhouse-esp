#include "sensors.h"

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SENSORS";

// ADC handle (oneshot mode)
static adc_oneshot_unit_handle_t adc_handle = NULL;

// Mutex for thread-safe sensor operations
static SemaphoreHandle_t sensor_mutex = NULL;

// Static sensor info array
// This stores configuration and metadata for each sensor
static sensor_info_t sensors[SENSOR_COUNT] = {
    [SENSOR_LIGHT_ROOF] = {.type = SENSOR_TYPE_LIGHT,
                           .channel = ADC_CHANNEL_0,  // GPIO0
                           .location = "roof",
                           .calib = {.type = CALIB_NONE, .unit = "raw"}},
    [SENSOR_WATER_ROOF] = {.type = SENSOR_TYPE_WATER,
                           .channel = ADC_CHANNEL_1,  // GPIO1
                           .location = "roof",
                           .calib = {.type = CALIB_NONE, .unit = "raw"}}};

/**
 * Apply linear calibration: y = mx + b
 *
 * @param raw Raw ADC value (0-4095)
 * @param calib Calibration parameters
 * @return Calibrated value
 */
static float apply_linear_calibration(int raw, const linear_calib_t *calib) {
    return calib->m * raw + calib->b;
}

/**
 * Apply polynomial calibration: y = ax^2 + bx + c
 *
 * @param raw Raw ADC value (0-4095)
 * @param calib Calibration parameters
 * @return Calibrated value
 */
static float apply_polynomial_calibration(int raw, const poly_calib_t *calib) {
    float x = (float) raw;
    return calib->a * x * x + calib->b * x + calib->c;
}

esp_err_t sensor_init(void) {
    ESP_LOGI(TAG, "Initializing sensor driver...");

    // Create mutex for thread safety
    sensor_mutex = xSemaphoreCreateMutex();
    if (sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // Create ADC oneshot handle for ADC1
    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t ret = adc_oneshot_new_unit(&adc_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure all sensor channels
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,          // 0-3.3V range
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 12-bit (0-4095)
    };

    for (int i = 0; i < SENSOR_COUNT; i++) {
        ret = adc_oneshot_config_channel(adc_handle, sensors[i].channel, &chan_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure channel %d: %s", sensors[i].channel,
                     esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "Sensor driver initialized (ADC1, 12-bit, 0-3.3V)");
    ESP_LOGI(TAG, "  Light sensor: GPIO0/CH0 (%s)", sensors[SENSOR_LIGHT_ROOF].location);
    ESP_LOGI(TAG, "  Water sensor: GPIO1/CH1 (%s)", sensors[SENSOR_WATER_ROOF].location);

    return ESP_OK;
}

esp_err_t sensor_read(sensor_id_t id, sensor_reading_t *reading) {
    // Input validation
    if (id >= SENSOR_COUNT || reading == NULL) {
        ESP_LOGE(TAG, "Invalid arguments (id=%d, reading=%p)", id, reading);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect ADC access
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Read raw ADC value
    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc_handle, sensors[id].channel, &raw_value);
    if (ret != ESP_OK) {
        xSemaphoreGive(sensor_mutex);
        ESP_LOGE(TAG, "Failed to read ADC channel %d: %s", sensors[id].channel,
                 esp_err_to_name(ret));
        return ret;
    }

    // Release mutex early (calibration doesn't need it)
    xSemaphoreGive(sensor_mutex);

    // Apply calibration
    float calibrated_value;
    switch (sensors[id].calib.type) {
        case CALIB_LINEAR:
            calibrated_value = apply_linear_calibration(raw_value, &sensors[id].calib.linear);
            break;

        case CALIB_POLYNOMIAL:
            calibrated_value = apply_polynomial_calibration(raw_value, &sensors[id].calib.poly);
            break;

        case CALIB_NONE:
        default:
            // No calibration - just use raw value
            calibrated_value = (float) raw_value;
            break;
    }

    // Get timestamp in milliseconds since boot
    uint32_t timestamp = (uint32_t) (esp_timer_get_time() / 1000);

    // Populate reading structure
    reading->id = id;
    reading->raw_value = raw_value;
    reading->calibrated_value = calibrated_value;
    reading->unit = sensors[id].calib.unit;
    reading->timestamp = timestamp;

    ESP_LOGD(TAG, "Sensor %d read: raw=%d, calib=%.2f %s, time=%lu ms", id, raw_value,
             calibrated_value, reading->unit, timestamp);

    return ESP_OK;
}

esp_err_t sensor_set_calibration(sensor_id_t id, const calibration_t *calib) {
    // Input validation
    if (id >= SENSOR_COUNT || calib == NULL) {
        ESP_LOGE(TAG, "Invalid arguments (id=%d, calib=%p)", id, calib);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect sensor config
    if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Copy calibration config
    sensors[id].calib = *calib;

    // Release mutex
    xSemaphoreGive(sensor_mutex);

    ESP_LOGI(TAG, "Sensor %d calibration updated: type=%d, unit=%s", id, calib->type, calib->unit);

    return ESP_OK;
}

const sensor_info_t *sensor_get_info(sensor_id_t id) {
    // Input validation
    if (id >= SENSOR_COUNT) {
        ESP_LOGE(TAG, "Invalid sensor ID: %d", id);
        return NULL;
    }

    // Return pointer to static info
    // No mutex needed - basic info is read-only
    // (calibration can change, but callers should handle that)
    return &sensors[id];
}
