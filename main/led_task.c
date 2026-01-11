#include "led_task.h"
#include "actuators.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LED_TASK";

void led_task(void *pvParameters)
{
    // We don't use parameters for this task
    (void)pvParameters;  // Silence unused parameter warning

    ESP_LOGI(TAG, "LED task started");
    ESP_LOGI(TAG, "Blinking LEDs alternately every 500ms...");

    // Task loop - runs forever
    // This task is independent and doesn't coordinate with other tasks
    while (1) {
        // Toggle both LEDs
        // They alternate because we toggle both at once:
        // If LED1 was ON and LED2 was OFF, now LED1 is OFF and LED2 is ON
        led_toggle(LED_YELLOW_ROOF);
        led_toggle(LED_WHITE_GARDEN);

        // Wait 500ms before next toggle
        // This gives us a visible blink at 1Hz (on for 500ms, off for 500ms)
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Note: This task never exits. If it did, we'd need vTaskDelete(NULL) here.
}
