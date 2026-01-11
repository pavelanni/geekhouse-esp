#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

/**
 * Display task
 *
 * Receives sensor readings from queue and prints to console.
 * This is a consumer task that blocks waiting for queue items.
 *
 * Task parameters:
 * - Priority: 4 (lower than sensor task)
 * - Stack: 3KB
 *
 * @param pvParameters Queue handle (QueueHandle_t) for sensor readings
 */
void display_task(void *pvParameters);

#endif // DISPLAY_TASK_H
