#ifndef STATS_TASK_H
#define STATS_TASK_H

/**
 * Statistics monitoring task
 *
 * Periodically prints task statistics including:
 * - Task states (Running, Blocked, Ready, Suspended, Deleted)
 * - Stack high water mark (minimum free stack)
 * - CPU usage percentage
 *
 * Task parameters:
 * - Priority: 2 (low - monitoring shouldn't interfere)
 * - Stack: 4KB (needs space for formatted strings)
 * - Period: 10 seconds
 *
 * @param pvParameters Unused (NULL)
 */
void stats_task(void *pvParameters);

#endif  // STATS_TASK_H
