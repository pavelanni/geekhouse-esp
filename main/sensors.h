#ifndef SENSORS_H
#define SENSORS_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

// Sensor types
typedef enum {
    SENSOR_TYPE_LIGHT,
    SENSOR_TYPE_WATER,
} sensor_type_t;

// Sensor identifiers
typedef enum {
    SENSOR_LIGHT_ROOF = 0,   // GPIO0, ADC1_CH0
    SENSOR_WATER_ROOF = 1,   // GPIO1, ADC1_CH1
    SENSOR_COUNT = 2
} sensor_id_t;

// Calibration type
typedef enum {
    CALIB_LINEAR,      // y = mx + b
    CALIB_POLYNOMIAL,  // y = ax^2 + bx + c
    CALIB_NONE         // Raw value only
} calib_type_t;

// Linear calibration params
typedef struct {
    float m;  // slope
    float b;  // intercept
} linear_calib_t;

// Polynomial calibration params
typedef struct {
    float a;  // x^2 coefficient
    float b;  // x coefficient
    float c;  // constant
} poly_calib_t;

// Calibration configuration
typedef struct {
    calib_type_t type;
    union {
        linear_calib_t linear;
        poly_calib_t poly;
    };
    const char *unit;  // e.g., "lux", "%"
} calibration_t;

// Sensor reading (for queue)
typedef struct {
    sensor_id_t id;
    int raw_value;        // 0-4095 (12-bit ADC)
    float calibrated_value;
    const char *unit;
    uint32_t timestamp;   // milliseconds since boot
} sensor_reading_t;

// Sensor metadata
typedef struct {
    sensor_type_t type;
    adc_channel_t channel;
    calibration_t calib;
    const char *location;
} sensor_info_t;

/**
 * Initialize all sensors
 *
 * Sets up ADC1 unit and configures all channels.
 * Default calibration is CALIB_NONE.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_init(void);

/**
 * Read sensor value
 *
 * Reads raw ADC, applies calibration, and populates reading struct.
 * Thread-safe - can be called from multiple tasks.
 *
 * @param id Sensor identifier
 * @param[out] reading Populated reading structure
 * @return ESP_OK on success
 */
esp_err_t sensor_read(sensor_id_t id, sensor_reading_t *reading);

/**
 * Set calibration for sensor
 *
 * @param id Sensor identifier
 * @param calib Calibration configuration
 * @return ESP_OK on success
 */
esp_err_t sensor_set_calibration(sensor_id_t id, const calibration_t *calib);

/**
 * Get sensor info
 *
 * @param id Sensor identifier
 * @return Pointer to sensor info, or NULL if invalid
 */
const sensor_info_t* sensor_get_info(sensor_id_t id);

#endif // SENSORS_H
