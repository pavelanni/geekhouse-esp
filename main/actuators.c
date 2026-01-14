#include "actuators.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ACTUATORS";

// Static LED info array
// This stores GPIO mapping and metadata for each LED
static led_info_t leds[LED_COUNT] = {
    [LED_YELLOW_ROOF] = {.gpio = 2, .state = false, .color = "yellow", .location = "roof"},
    [LED_WHITE_GARDEN] = {.gpio = 3, .state = false, .color = "white", .location = "garden"}};

// Mutex for thread-safe state access
// This protects the leds[] array from concurrent modification
static SemaphoreHandle_t led_mutex = NULL;

esp_err_t led_init(void) {
    ESP_LOGI(TAG, "Initializing LED driver...");

    // Create mutex for thread safety
    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // Configure all LED GPIOs as outputs
    gpio_config_t led_conf = {.pin_bit_mask = (1ULL << leds[LED_YELLOW_ROOF].gpio) |
                                              (1ULL << leds[LED_WHITE_GARDEN].gpio),
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};

    esp_err_t ret = gpio_config(&led_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize all LEDs to OFF
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_set_level(leds[i].gpio, 0);
        leds[i].state = false;
    }

    ESP_LOGI(TAG, "LED driver initialized (GPIO2: %s/%s, GPIO3: %s/%s)",
             leds[LED_YELLOW_ROOF].color, leds[LED_YELLOW_ROOF].location,
             leds[LED_WHITE_GARDEN].color, leds[LED_WHITE_GARDEN].location);

    return ESP_OK;
}

esp_err_t led_on(led_id_t id) {
    // Input validation
    if (id >= LED_COUNT) {
        ESP_LOGE(TAG, "Invalid LED ID: %d", id);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect state
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Turn on LED
    gpio_set_level(leds[id].gpio, 1);
    leds[id].state = true;

    // Release mutex
    xSemaphoreGive(led_mutex);

    ESP_LOGD(TAG, "LED %d (%s) turned ON", id, leds[id].color);
    return ESP_OK;
}

esp_err_t led_off(led_id_t id) {
    // Input validation
    if (id >= LED_COUNT) {
        ESP_LOGE(TAG, "Invalid LED ID: %d", id);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect state
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Turn off LED
    gpio_set_level(leds[id].gpio, 0);
    leds[id].state = false;

    // Release mutex
    xSemaphoreGive(led_mutex);

    ESP_LOGD(TAG, "LED %d (%s) turned OFF", id, leds[id].color);
    return ESP_OK;
}

esp_err_t led_toggle(led_id_t id) {
    // Input validation
    if (id >= LED_COUNT) {
        ESP_LOGE(TAG, "Invalid LED ID: %d", id);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect state
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Toggle LED state
    leds[id].state = !leds[id].state;
    gpio_set_level(leds[id].gpio, leds[id].state ? 1 : 0);

    // Release mutex
    xSemaphoreGive(led_mutex);

    ESP_LOGD(TAG, "LED %d (%s) toggled to %s", id, leds[id].color, leds[id].state ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t led_get_state(led_id_t id, bool *state) {
    // Input validation
    if (id >= LED_COUNT || state == NULL) {
        ESP_LOGE(TAG, "Invalid arguments (id=%d, state=%p)", id, state);
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex to protect state
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Read state
    *state = leds[id].state;

    // Release mutex
    xSemaphoreGive(led_mutex);

    return ESP_OK;
}

const led_info_t *led_get_info(led_id_t id) {
    // Input validation
    if (id >= LED_COUNT) {
        ESP_LOGE(TAG, "Invalid LED ID: %d", id);
        return NULL;
    }

    // Return pointer to static info
    // No mutex needed - info is read-only after init
    return &leds[id];
}
