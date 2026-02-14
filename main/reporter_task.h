#ifndef REPORTER_TASK_H
#define REPORTER_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * Reporter task
 *
 * Waits for both sensors to have fresh readings (using event group),
 * then calculates and reports summary statistics.
 *
 * Task parameters:
 * - Priority: 4 (same as display task)
 * - Stack: 4KB
 * - Period: After every 10 sensor reading cycles
 *
 * @param pvParameters Event group handle (EventGroupHandle_t)
 */
void reporter_task(void *pvParameters);

// Event group bits
#define LIGHT_SENSOR_READY_BIT (1 << 0)  // Bit 0: Light sensor has new reading
#define WATER_SENSOR_READY_BIT (1 << 1)  // Bit 1: Water sensor has new reading
#define ALL_SENSORS_READY_BITS (LIGHT_SENSOR_READY_BIT | WATER_SENSOR_READY_BIT)

#endif  // REPORTER_TASK_H
