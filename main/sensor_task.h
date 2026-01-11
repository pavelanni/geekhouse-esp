#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

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

#endif // SENSOR_TASK_H
