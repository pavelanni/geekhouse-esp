#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
/**
 * Sensor reading task
 *
 * Periodically reads all sensors and pushes readings to queue.
 *
 * Task parameters:
 * - Priority: 5 (medium)
 * - Stack: 4KB
 * - Period: 2 seconds
 *
 * @param pvParameters Queue handle (QueueHandle_t) for sensor readings
 */
void sensor_task(void *pvParameters);

// The queue and event group were created in app_main() and passed to us
// Cast parameter to structure containing both handles
typedef struct {
    QueueHandle_t queue;
    EventGroupHandle_t events;
} sensor_task_params_t;
#endif  // SENSOR_TASK_H
