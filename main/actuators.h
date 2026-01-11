#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdbool.h>
#include "esp_err.h"

// LED identifiers
typedef enum {
    LED_YELLOW_ROOF = 0,  // GPIO2
    LED_WHITE_GARDEN = 1, // GPIO3
    LED_COUNT = 2
} led_id_t;

// LED state structure
typedef struct {
    int gpio;
    bool state;
    const char *color;
    const char *location;
} led_info_t;

/**
 * Initialize all LEDs
 *
 * Sets up GPIO pins as outputs and initializes state to OFF.
 * Must be called before any other LED functions.
 *
 * @return ESP_OK on success
 */
esp_err_t led_init(void);

/**
 * Turn LED on
 *
 * @param id LED identifier
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if id invalid
 */
esp_err_t led_on(led_id_t id);

/**
 * Turn LED off
 *
 * @param id LED identifier
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if id invalid
 */
esp_err_t led_off(led_id_t id);

/**
 * Toggle LED state
 *
 * @param id LED identifier
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if id invalid
 */
esp_err_t led_toggle(led_id_t id);

/**
 * Get LED state
 *
 * @param id LED identifier
 * @param[out] state Current state (true=on, false=off)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if id invalid
 */
esp_err_t led_get_state(led_id_t id, bool *state);

/**
 * Get LED info (for REST API)
 *
 * @param id LED identifier
 * @return Pointer to LED info struct, or NULL if invalid
 */
const led_info_t* led_get_info(led_id_t id);

#endif // ACTUATORS_H
