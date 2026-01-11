#ifndef LED_TASK_H
#define LED_TASK_H

/**
 * LED blink task
 *
 * Simple demo task that alternately blinks the two LEDs.
 * Shows how tasks can run independently without coordination.
 *
 * Later, this will be replaced by HTTP/MQTT control where
 * other tasks send commands to control LEDs.
 *
 * Task parameters:
 * - Priority: 3 (low - visual indicator only)
 * - Stack: 2KB
 * - Period: 500ms toggle
 *
 * @param pvParameters Unused (NULL)
 */
void led_task(void *pvParameters);

#endif // LED_TASK_H
