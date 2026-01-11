# Phase 2 Learning Tasks - FreeRTOS Deep Dive

This document contains hands-on learning tasks to deepen your understanding of FreeRTOS and ESP-IDF concepts. Each task builds on the Phase 2 multi-task architecture you've already implemented.

**Recommended approach:** Complete tasks in order from easiest to hardest. Each task introduces important concepts you'll use in real-world embedded development.

---

## Task 1: Task Statistics Monitor (Easy - 20 mins)

### Learning Goal
Understand FreeRTOS task monitoring and runtime statistics. Learn how to track task behavior, CPU usage, and stack consumption.

### What to Build
Create a monitoring task that periodically prints statistics about all running tasks:
- Task names and their current states (Running, Blocked, Ready, Suspended)
- Stack high water mark (minimum free stack space ever reached)
- CPU usage percentage per task

### Why This Matters
- **Debugging**: See which tasks are consuming CPU
- **Optimization**: Identify over-allocated stack sizes
- **Profiling**: Find performance bottlenecks
- **Production**: Monitor system health in deployed devices

### Implementation Steps

#### Step 1: Enable Task Statistics in menuconfig

```bash
idf.py menuconfig
```

Navigate through:
1. `Component config` → `FreeRTOS` → `Kernel`
2. Enable these options:
   - `configUSE_TRACE_FACILITY` - Enables task state tracking
   - `configGENERATE_RUN_TIME_STATS` - Enables CPU usage tracking
   - `configUSE_STATS_FORMATTING_FUNCTIONS` - Enables vTaskList() and vTaskGetRunTimeStats()

Save and exit menuconfig.

#### Step 2: Create stats_task.h

```c
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

#endif // STATS_TASK_H
```

#### Step 3: Create stats_task.c

```c
#include "stats_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "STATS_TASK";

// Buffer sizes for task statistics
#define STATS_BUFFER_SIZE 1024

void stats_task(void *pvParameters)
{
    (void)pvParameters;

    // Allocate buffers for statistics strings
    // These are large, so allocate from heap rather than stack
    char *task_list_buffer = malloc(STATS_BUFFER_SIZE);
    char *cpu_stats_buffer = malloc(STATS_BUFFER_SIZE);

    if (task_list_buffer == NULL || cpu_stats_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate statistics buffers");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Statistics task started");
    ESP_LOGI(TAG, "Printing task stats every 10 seconds...");
    ESP_LOGI(TAG, "");

    while (1) {
        // Wait 10 seconds before printing stats
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========== TASK STATISTICS ==========");
        ESP_LOGI(TAG, "");

        // Get task state information
        // Format: Name  State  Priority  Stack  TaskNum
        ESP_LOGI(TAG, "Task States (X=Running, B=Blocked, R=Ready, S=Suspended, D=Deleted):");
        vTaskList(task_list_buffer);
        ESP_LOGI(TAG, "%s", task_list_buffer);

        ESP_LOGI(TAG, "");

        // Get CPU usage statistics
        // Format: Name  AbsTime  %%Time
        ESP_LOGI(TAG, "CPU Usage:");
        vTaskGetRunTimeStats(cpu_stats_buffer);
        ESP_LOGI(TAG, "%s", cpu_stats_buffer);

        ESP_LOGI(TAG, "");

        // Print free heap size
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "Minimum free heap (since boot): %lu bytes", esp_get_minimum_free_heap_size());

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=====================================");
        ESP_LOGI(TAG, "");
    }

    // Cleanup (never reached, but good practice)
    free(task_list_buffer);
    free(cpu_stats_buffer);
    vTaskDelete(NULL);
}
```

#### Step 4: Update main.c

Add the stats task to your main.c:

```c
#include "stats_task.h"  // Add to includes

// In app_main(), after creating other tasks:

ESP_LOGI(TAG, "  Creating stats_task (priority: 2, stack: 4KB)...");
ret = xTaskCreate(
    stats_task,         // Task function
    "stats",            // Task name
    4096,               // Stack size (needs space for buffers)
    NULL,               // No parameters
    2,                  // Priority (lower than all functional tasks)
    NULL                // Task handle
);
if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create stats task");
    return;
}
```

#### Step 5: Update CMakeLists.txt

Add the new source file:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "actuators.c"
        "sensors.c"
        "sensor_task.c"
        "display_task.c"
        "led_task.c"
        "stats_task.c"      # Add this line
    INCLUDE_DIRS "."
)
```

#### Step 6: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Output

Every 10 seconds you should see:

```
I (10234) STATS_TASK:
I (10234) STATS_TASK: ========== TASK STATISTICS ==========
I (10234) STATS_TASK:
I (10234) STATS_TASK: Task States (X=Running, B=Blocked, R=Ready, S=Suspended, D=Deleted):
I (10234) STATS_TASK: sensor          B       5       3124    2
I (10234) STATS_TASK: display         B       4       2456    3
I (10234) STATS_TASK: led             B       3       1892    4
I (10234) STATS_TASK: stats           R       2       3048    5
I (10234) STATS_TASK: IDLE            R       0       512     6
I (10234) STATS_TASK:
I (10234) STATS_TASK: CPU Usage:
I (10234) STATS_TASK: sensor          1245    2%
I (10234) STATS_TASK: display         987     1%
I (10234) STATS_TASK: led             654     1%
I (10234) STATS_TASK: stats           432     <1%
I (10234) STATS_TASK: IDLE            58234   95%
I (10234) STATS_TASK:
I (10234) STATS_TASK: Free heap: 245632 bytes
I (10234) STATS_TASK: Minimum free heap (since boot): 243104 bytes
```

### Learning Points

1. **Task States**:
   - `B` (Blocked) - Task is waiting (on queue, delay, semaphore, etc.)
   - `R` (Ready) - Task is ready to run but lower priority task is running
   - `X` (Running) - Task is currently executing
   - Most tasks spend most time in Blocked state (efficient!)

2. **Stack High Water Mark**:
   - Shows minimum free stack ever reached
   - If a task shows 100 bytes free, its stack is too small!
   - Safe margin: at least 512-1024 bytes free
   - Use this to right-size your task stacks

3. **CPU Usage**:
   - IDLE task consuming 90%+ CPU is GOOD - means you have spare capacity
   - If any task shows high % but should be blocked, investigate why
   - Total doesn't always sum to 100% due to rounding and interrupts

4. **Heap Monitoring**:
   - Free heap decreasing over time = memory leak
   - Minimum free heap shows worst-case memory pressure
   - Watch for fragmentation (many small allocations)

### Extension Ideas

Once working, try:
- Add heap usage per task (requires `configUSE_TRACE_FACILITY`)
- Graph CPU usage over time
- Set thresholds and alert when stack/heap is low
- Export stats over HTTP for remote monitoring

---

## Task 2: Software Timer for LED Blinking (Medium - 30 mins)

### Learning Goal
Use FreeRTOS software timers instead of dedicated tasks. Learn when timers are more appropriate than tasks.

### What to Build
Replace the `led_task` with a software timer callback. The LED blinking behavior stays the same, but implemented more efficiently.

### Why This Matters
- **Memory Efficiency**: Timers don't need their own stack (saves 2KB+ per timer)
- **Resource Sharing**: Multiple timers share one timer daemon task
- **Cleaner Code**: No need for infinite loops with vTaskDelay()
- **Best Practice**: Use timers for simple periodic operations

### When to Use Timers vs Tasks

**Use Software Timers when:**
- Operation is simple and quick (< 10ms)
- No blocking operations needed
- Periodic or one-shot execution
- Examples: LED blinking, polling, timeouts

**Use Tasks when:**
- Operation needs to block (queues, mutexes, delays)
- Operation takes significant time
- Need separate priority control
- Examples: sensor reading with delays, network communication

### Implementation Steps

#### Step 1: Remove LED Task Files

```bash
cd /home/pavel/esp/geekhouse-esp/main
rm led_task.c led_task.h
```

#### Step 2: Update CMakeLists.txt

Remove `led_task.c` from the SRCS list:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "actuators.c"
        "sensors.c"
        "sensor_task.c"
        "display_task.c"
        # "led_task.c"  <- Remove this line
        "stats_task.c"
    INCLUDE_DIRS "."
)
```

#### Step 3: Update main.c

Replace the led_task with a timer:

```c
// Remove this include:
// #include "led_task.h"

// Add this include:
#include "freertos/timers.h"

// Add timer callback function before app_main():

/**
 * LED timer callback
 *
 * This callback is executed by the timer daemon task every 500ms.
 * It toggles both LEDs, creating an alternating blink pattern.
 *
 * IMPORTANT: Timer callbacks must be quick and non-blocking!
 * - Don't use vTaskDelay()
 * - Don't block on queues with long timeouts
 * - Don't do heavy processing
 *
 * If you need blocking operations, use a task instead.
 */
static void led_timer_callback(TimerHandle_t xTimer)
{
    // Unused parameter
    (void)xTimer;

    // Toggle both LEDs
    // The actuator driver is thread-safe (uses mutex internally)
    led_toggle(LED_YELLOW_ROOF);
    led_toggle(LED_WHITE_GARDEN);
}

void app_main(void)
{
    // ... existing initialization code ...

    // Replace the LED task creation with timer creation:

    // Create LED blink timer (instead of led_task)
    ESP_LOGI(TAG, "  Creating led_timer (period: 500ms)...");
    TimerHandle_t led_timer = xTimerCreate(
        "led_blink",              // Timer name (for debugging)
        pdMS_TO_TICKS(500),       // Period: 500ms
        pdTRUE,                   // Auto-reload: timer repeats automatically
        NULL,                     // Timer ID: not used
        led_timer_callback        // Callback function
    );

    if (led_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED timer");
        return;
    }

    // Start the timer
    // Second parameter is block time: 0 means don't wait if timer queue is full
    if (xTimerStart(led_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start LED timer");
        return;
    }

    // ... rest of app_main() ...
}
```

#### Step 4: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Result

LEDs should blink exactly as before, but now you'll see in task statistics:
- No "led" task listed
- Timer daemon task (`Tmr Svc`) handling the LED callback
- ~2KB less RAM usage (no LED task stack)

### Learning Points

1. **Timer Daemon Task**:
   - FreeRTOS creates one "Tmr Svc" task automatically
   - All timer callbacks execute in this task's context
   - Default priority: usually above IDLE but below application tasks
   - Shared stack saves memory

2. **Auto-Reload vs One-Shot**:
   - Auto-reload (`pdTRUE`): Timer restarts automatically (for periodic operations)
   - One-shot (`pdFALSE`): Timer fires once then stops (for timeouts)

3. **Timer Callback Rules**:
   - Keep callbacks SHORT (< 10ms recommended)
   - NO blocking operations (`vTaskDelay`, long mutex waits)
   - NO heavy processing
   - If you need to do complex work, send message to a task instead

4. **Timer Commands**:
   - `xTimerStart()` - Start or restart a timer
   - `xTimerStop()` - Stop a running timer
   - `xTimerChangePeriod()` - Change the period of a timer
   - `xTimerReset()` - Restart timer from current time
   - Commands go through a queue (may fail if queue is full)

### Extension Ideas

Once working, try:
- Create multiple timers with different periods
- Make one timer one-shot for a delayed startup
- Change timer period dynamically based on sensor readings
- Use timer ID to pass context to callback

---

## Task 3: Event Groups for Sensor Synchronization (Medium-Hard - 45 mins)

### Learning Goal
Use event groups to wait for multiple conditions. Learn to coordinate tasks that depend on multiple events.

### What to Build
Create a "reporter" task that waits for BOTH sensors to have fresh readings, then calculates and reports summary statistics (min, max, average over last 10 readings).

### Why This Matters
- **Coordination**: Wait for multiple conditions before proceeding
- **Synchronization**: Ensure data consistency across tasks
- **Real-World Use**: Common pattern for startup sequences, multi-sensor fusion, state machines
- **Examples**: Wait for WiFi + NTP before starting web server, wait for all sensors before calibration

### Event Groups Explained

**Event Groups** are a set of binary flags (bits) that tasks can:
- **Set**: Mark an event as having occurred
- **Clear**: Mark an event as not occurred
- **Wait**: Block until one or more events occur

Think of them as a set of 24 light switches (bits 0-23) that tasks can flip and watch.

**Wait Modes**:
- **OR**: Wake when ANY bit is set (`xWaitForAnyBits = pdFALSE`)
- **AND**: Wake when ALL bits are set (`xWaitForAllBits = pdTRUE`)

### Implementation Steps

#### Step 1: Create reporter_task.h

```c
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

#endif // REPORTER_TASK_H
```

#### Step 2: Create Shared Data Structure

Create `sensor_data_shared.h`:

```c
#ifndef SENSOR_DATA_SHARED_H
#define SENSOR_DATA_SHARED_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Shared sensor data structure
typedef struct {
    int light_raw;
    float light_calibrated;
    int water_raw;
    float water_calibrated;
    uint32_t timestamp;
} shared_sensor_data_t;

// Global shared data (protected by mutex)
extern shared_sensor_data_t g_shared_sensor_data;
extern SemaphoreHandle_t g_shared_data_mutex;

#endif // SENSOR_DATA_SHARED_H
```

Create `sensor_data_shared.c`:

```c
#include "sensor_data_shared.h"

// Global shared sensor data
shared_sensor_data_t g_shared_sensor_data = {0};
SemaphoreHandle_t g_shared_data_mutex = NULL;
```

#### Step 3: Update CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "main.c"
        "actuators.c"
        "sensors.c"
        "sensor_task.c"
        "display_task.c"
        "stats_task.c"
        "sensor_data_shared.c"    # Add this
        "reporter_task.c"          # Add this (will create next)
    INCLUDE_DIRS "."
)
```

#### Step 4: Initialize Shared Resources in main.c

```c
#include "reporter_task.h"
#include "sensor_data_shared.h"

void app_main(void)
{
    // ... existing initialization ...

    // Create mutex for shared sensor data
    ESP_LOGI(TAG, "Creating shared data mutex...");
    g_shared_data_mutex = xSemaphoreCreateMutex();
    if (g_shared_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create shared data mutex");
        return;
    }

    // Create event group for sensor coordination
    ESP_LOGI(TAG, "Creating sensor event group...");
    EventGroupHandle_t sensor_events = xEventGroupCreate();
    if (sensor_events == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // ... create sensor_queue as before ...

    // Update sensor_task creation to pass both queue AND event group
    // We'll need to create a structure to pass both:
    typedef struct {
        QueueHandle_t queue;
        EventGroupHandle_t events;
    } sensor_task_params_t;

    static sensor_task_params_t sensor_params = {
        .queue = sensor_queue,
        .events = sensor_events
    };

    ret = xTaskCreate(
        sensor_task,
        "sensor",
        4096,
        &sensor_params,    // Pass structure with both handles
        5,
        NULL
    );

    // Create reporter task
    ESP_LOGI(TAG, "  Creating reporter_task (priority: 4, stack: 4KB)...");
    ret = xTaskCreate(
        reporter_task,
        "reporter",
        4096,
        sensor_events,     // Pass event group
        4,
        NULL
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reporter task");
        return;
    }

    // ... rest of app_main() ...
}
```

#### Step 5: Update sensor_task.c

```c
#include "sensor_data_shared.h"
#include "reporter_task.h"  // For event bit definitions

// Update function signature to accept structure
void sensor_task(void *pvParameters)
{
    // Cast parameter to structure containing both handles
    typedef struct {
        QueueHandle_t queue;
        EventGroupHandle_t events;
    } sensor_task_params_t;

    sensor_task_params_t *params = (sensor_task_params_t *)pvParameters;
    QueueHandle_t queue = params->queue;
    EventGroupHandle_t events = params->events;

    sensor_reading_t reading;

    ESP_LOGI(TAG, "Sensor task started");

    while (1) {
        // Read light sensor
        if (sensor_read(SENSOR_LIGHT_ROOF, &reading) == ESP_OK) {
            // Send to queue as before
            if (xQueueSend(queue, &reading, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Queue full, dropping light reading");
            }

            // Update shared data structure
            if (xSemaphoreTake(g_shared_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_shared_sensor_data.light_raw = reading.raw_value;
                g_shared_sensor_data.light_calibrated = reading.calibrated_value;
                g_shared_sensor_data.timestamp = reading.timestamp;
                xSemaphoreGive(g_shared_data_mutex);

                // Signal that light sensor has new data
                xEventGroupSetBits(events, LIGHT_SENSOR_READY_BIT);
            }
        }

        // Read water sensor
        if (sensor_read(SENSOR_WATER_ROOF, &reading) == ESP_OK) {
            // Send to queue as before
            if (xQueueSend(queue, &reading, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Queue full, dropping water reading");
            }

            // Update shared data structure
            if (xSemaphoreTake(g_shared_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                g_shared_sensor_data.water_raw = reading.raw_value;
                g_shared_sensor_data.water_calibrated = reading.calibrated_value;
                xSemaphoreGive(g_shared_data_mutex);

                // Signal that water sensor has new data
                xEventGroupSetBits(events, WATER_SENSOR_READY_BIT);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

#### Step 6: Create reporter_task.c

```c
#include "reporter_task.h"
#include "sensor_data_shared.h"
#include "esp_log.h"

static const char *TAG = "REPORTER";

#define HISTORY_SIZE 10

// Statistics structure
typedef struct {
    int light_min;
    int light_max;
    float light_sum;
    int water_min;
    int water_max;
    float water_sum;
    int count;
} sensor_stats_t;

void reporter_task(void *pvParameters)
{
    EventGroupHandle_t events = (EventGroupHandle_t)pvParameters;
    sensor_stats_t stats = {0};

    // Initialize min values to max possible
    stats.light_min = 4095;
    stats.water_min = 4095;

    ESP_LOGI(TAG, "Reporter task started");
    ESP_LOGI(TAG, "Waiting for sensor readings...");

    while (1) {
        // Wait for BOTH sensors to have new data
        // Parameters:
        //   - events: Event group handle
        //   - ALL_SENSORS_READY_BITS: Bits to wait for
        //   - pdTRUE: Clear bits on exit (so we wait for next reading)
        //   - pdTRUE: Wait for ALL bits (AND logic, not OR)
        //   - portMAX_DELAY: Wait forever
        EventBits_t bits = xEventGroupWaitBits(
            events,
            ALL_SENSORS_READY_BITS,
            pdTRUE,          // Clear bits on exit
            pdTRUE,          // Wait for all bits (AND)
            portMAX_DELAY    // Wait forever
        );

        // Both sensors have new data!
        // Read from shared structure
        if (xSemaphoreTake(g_shared_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            int light = g_shared_sensor_data.light_raw;
            int water = g_shared_sensor_data.water_raw;
            xSemaphoreGive(g_shared_data_mutex);

            // Update statistics
            if (light < stats.light_min) stats.light_min = light;
            if (light > stats.light_max) stats.light_max = light;
            stats.light_sum += light;

            if (water < stats.water_min) stats.water_min = water;
            if (water > stats.water_max) stats.water_max = water;
            stats.water_sum += water;

            stats.count++;

            // Print summary every 10 readings
            if (stats.count >= HISTORY_SIZE) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "===== Sensor Summary (last %d readings) =====", HISTORY_SIZE);
                ESP_LOGI(TAG, "  Light: min=%d, max=%d, avg=%.0f",
                         stats.light_min,
                         stats.light_max,
                         stats.light_sum / HISTORY_SIZE);
                ESP_LOGI(TAG, "  Water: min=%d, max=%d, avg=%.0f",
                         stats.water_min,
                         stats.water_max,
                         stats.water_sum / HISTORY_SIZE);
                ESP_LOGI(TAG, "==========================================");
                ESP_LOGI(TAG, "");

                // Reset statistics
                stats.light_min = 4095;
                stats.light_max = 0;
                stats.light_sum = 0;
                stats.water_min = 4095;
                stats.water_max = 0;
                stats.water_sum = 0;
                stats.count = 0;
            }
        } else {
            ESP_LOGW(TAG, "Failed to acquire mutex");
        }
    }
}
```

#### Step 7: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Output

Every 20 seconds (10 readings × 2 seconds), you should see:

```
I (20000) REPORTER:
I (20000) REPORTER: ===== Sensor Summary (last 10 readings) =====
I (20000) REPORTER:   Light: min=2800, max=3100, avg=2950
I (20000) REPORTER:   Water: min=1500, max=1620, avg=1558
I (20000) REPORTER: ==========================================
```

### Learning Points

1. **Event Group Syntax**:
   ```c
   // Wait for ANY bit (OR logic):
   xEventGroupWaitBits(events, bits, pdTRUE, pdFALSE, timeout);

   // Wait for ALL bits (AND logic):
   xEventGroupWaitBits(events, bits, pdTRUE, pdTRUE, timeout);

   // Don't clear bits on exit:
   xEventGroupWaitBits(events, bits, pdFALSE, pdTRUE, timeout);
   ```

2. **Shared Data Pattern**:
   - Use mutex to protect shared structure
   - Event group signals "data is ready"
   - Consumer waits on event, then reads with mutex

3. **Event Groups vs Queues**:
   - **Event Groups**: Signal that something happened (boolean flags)
   - **Queues**: Transfer actual data between tasks
   - Often used together: event signals data ready, queue contains the data

4. **Typical Use Cases**:
   - Startup synchronization (wait for WiFi + NTP + config loaded)
   - Multi-sensor fusion (wait for all sensors before calculating)
   - State machines (wait for multiple conditions before transition)
   - Graceful shutdown (wait for all tasks to acknowledge shutdown)

### Extension Ideas

- Add more statistics (standard deviation, median)
- Add event bit for "calibration needed" condition
- Create startup event group (WiFi ready, sensors ready, etc.)
- Add error detection (timeout if sensor doesn't update)

---

## Task 4: Simple Console Command System (Hard - 60 mins)

### Learning Goal
Implement interactive control using ESP-IDF console component. Learn command parsing, argument handling, and interactive debugging techniques.

### What to Build
Add a console task that accepts typed commands via serial to control the system:
- `led on <id>` - Turn LED on
- `led off <id>` - Turn LED off
- `sensor read <id>` - Force immediate sensor reading
- `sensor calib <id> <m> <b>` - Set linear calibration
- `stats` - Print task statistics

### Why This Matters
- **Interactive Debugging**: Control system without reflashing
- **Production Testing**: Manufacturing test commands
- **Development Speed**: Quickly test features without hardcoding
- **Remote Control**: Can be adapted for network control

### Implementation Steps

#### Step 1: Update CMakeLists.txt

Add console component dependencies:

```cmake
idf_component_register(
    SRCS
        "main.c"
        "actuators.c"
        "sensors.c"
        "sensor_task.c"
        "display_task.c"
        "stats_task.c"
        "sensor_data_shared.c"
        "reporter_task.c"
        "console_task.c"          # Add this
    INCLUDE_DIRS "."
    PRIV_REQUIRES console vfs      # Add this line
)
```

#### Step 2: Create console_task.h

```c
#ifndef CONSOLE_TASK_H
#define CONSOLE_TASK_H

/**
 * Console task
 *
 * Provides interactive command-line interface via serial console.
 * Accepts commands to control LEDs, read sensors, set calibration, etc.
 *
 * Task parameters:
 * - Priority: 3 (medium)
 * - Stack: 4KB (console needs buffer space)
 *
 * @param pvParameters Unused (NULL)
 */
void console_task(void *pvParameters);

#endif // CONSOLE_TASK_H
```

#### Step 3: Create console_task.c

```c
#include "console_task.h"
#include "actuators.h"
#include "sensors.h"
#include "esp_console.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include <string.h>

static const char *TAG = "CONSOLE";

// LED command implementation
static int cmd_led(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: led <on|off|toggle> <id>\n");
        printf("  id: 0=yellow/roof, 1=white/garden\n");
        return 1;
    }

    const char *action = argv[1];
    int id = atoi(argv[2]);

    if (id < 0 || id >= LED_COUNT) {
        printf("Error: Invalid LED ID %d (must be 0-%d)\n", id, LED_COUNT - 1);
        return 1;
    }

    esp_err_t ret;
    if (strcmp(action, "on") == 0) {
        ret = led_on(id);
        printf("LED %d turned ON\n", id);
    } else if (strcmp(action, "off") == 0) {
        ret = led_off(id);
        printf("LED %d turned OFF\n", id);
    } else if (strcmp(action, "toggle") == 0) {
        ret = led_toggle(id);
        bool state;
        led_get_state(id, &state);
        printf("LED %d toggled to %s\n", id, state ? "ON" : "OFF");
    } else {
        printf("Error: Unknown action '%s' (use: on, off, toggle)\n", action);
        return 1;
    }

    if (ret != ESP_OK) {
        printf("Error: LED operation failed\n");
        return 1;
    }

    return 0;
}

// Sensor read command implementation
static int cmd_sensor_read(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: sensor read <id>\n");
        printf("  id: 0=light/roof, 1=water/roof\n");
        return 1;
    }

    int id = atoi(argv[1]);

    if (id < 0 || id >= SENSOR_COUNT) {
        printf("Error: Invalid sensor ID %d (must be 0-%d)\n", id, SENSOR_COUNT - 1);
        return 1;
    }

    sensor_reading_t reading;
    esp_err_t ret = sensor_read(id, &reading);

    if (ret != ESP_OK) {
        printf("Error: Failed to read sensor %d\n", id);
        return 1;
    }

    const sensor_info_t *info = sensor_get_info(id);
    printf("Sensor %d (%s, %s):\n", id,
           info->type == SENSOR_TYPE_LIGHT ? "light" : "water",
           info->location);
    printf("  Raw: %d\n", reading.raw_value);
    printf("  Calibrated: %.2f %s\n", reading.calibrated_value, reading.unit);
    printf("  Timestamp: %lu ms\n", reading.timestamp);

    return 0;
}

// Sensor calibration command implementation
static int cmd_sensor_calib(int argc, char **argv)
{
    if (argc < 5) {
        printf("Usage: sensor calib <id> <m> <b> <unit>\n");
        printf("  Sets linear calibration: y = m*x + b\n");
        printf("  id: 0=light/roof, 1=water/roof\n");
        printf("  m: slope (float)\n");
        printf("  b: intercept (float)\n");
        printf("  unit: unit string (e.g., 'lux', '%%', 'ppm')\n");
        return 1;
    }

    int id = atoi(argv[1]);
    float m = atof(argv[2]);
    float b = atof(argv[3]);
    const char *unit = argv[4];

    if (id < 0 || id >= SENSOR_COUNT) {
        printf("Error: Invalid sensor ID %d (must be 0-%d)\n", id, SENSOR_COUNT - 1);
        return 1;
    }

    calibration_t calib = {
        .type = CALIB_LINEAR,
        .linear = {.m = m, .b = b},
        .unit = unit
    };

    esp_err_t ret = sensor_set_calibration(id, &calib);

    if (ret != ESP_OK) {
        printf("Error: Failed to set calibration\n");
        return 1;
    }

    printf("Sensor %d calibration updated:\n", id);
    printf("  Formula: y = %.3f * x + %.3f\n", m, b);
    printf("  Unit: %s\n", unit);

    return 0;
}

// Stats command implementation
static int cmd_stats(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char *task_buffer = malloc(1024);
    char *cpu_buffer = malloc(1024);

    if (task_buffer && cpu_buffer) {
        printf("\n=== Task States ===\n");
        vTaskList(task_buffer);
        printf("%s\n", task_buffer);

        printf("=== CPU Usage ===\n");
        vTaskGetRunTimeStats(cpu_buffer);
        printf("%s\n", cpu_buffer);

        printf("Free heap: %lu bytes\n", esp_get_free_heap_size());
    }

    free(task_buffer);
    free(cpu_buffer);

    return 0;
}

// Help command implementation
static int cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\nAvailable commands:\n");
    printf("  led <on|off|toggle> <id>     - Control LED\n");
    printf("  sensor read <id>              - Read sensor value\n");
    printf("  sensor calib <id> <m> <b> <u> - Set linear calibration\n");
    printf("  stats                         - Show task statistics\n");
    printf("  help                          - Show this help\n");
    printf("\n");

    return 0;
}

void console_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Console task started");

    // Initialize console
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "geekhouse> ";
    repl_config.max_cmdline_length = 256;

    // Initialize USB CDC console (ESP32-C3 uses USB Serial/JTAG)
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_console_new_repl_uart(&uart_config, &repl_config, &repl);

    // Register commands
    const esp_console_cmd_t led_cmd = {
        .command = "led",
        .help = "Control LEDs (on/off/toggle)",
        .hint = NULL,
        .func = &cmd_led,
    };
    esp_console_cmd_register(&led_cmd);

    const esp_console_cmd_t sensor_cmd = {
        .command = "sensor",
        .help = "Sensor operations (read/calib)",
        .hint = NULL,
        .func = &cmd_sensor_read,  // Will parse subcommand
    };
    esp_console_cmd_register(&sensor_cmd);

    const esp_console_cmd_t stats_cmd = {
        .command = "stats",
        .help = "Show task statistics",
        .hint = NULL,
        .func = &cmd_stats,
    };
    esp_console_cmd_register(&stats_cmd);

    const esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "Show available commands",
        .hint = NULL,
        .func = &cmd_help,
    };
    esp_console_cmd_register(&help_cmd);

    // Print welcome message
    printf("\n");
    printf("====================================\n");
    printf("  Geekhouse Console Interface\n");
    printf("====================================\n");
    printf("Type 'help' for available commands\n");
    printf("\n");

    // Start console REPL (Read-Eval-Print Loop)
    // This function blocks and handles user input
    esp_console_start_repl(repl);

    // Should never reach here
    vTaskDelete(NULL);
}
```

#### Step 4: Update main.c

Add console task creation:

```c
#include "console_task.h"

// In app_main(), after other tasks:

ESP_LOGI(TAG, "  Creating console_task (priority: 3, stack: 4KB)...");
ret = xTaskCreate(
    console_task,
    "console",
    4096,
    NULL,
    3,
    NULL
);
if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create console task");
    return;
}
```

#### Step 5: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Interaction

```
geekhouse> help

Available commands:
  led <on|off|toggle> <id>     - Control LED
  sensor read <id>              - Read sensor value
  sensor calib <id> <m> <b> <u> - Set linear calibration
  stats                         - Show task statistics
  help                          - Show this help

geekhouse> led on 0
LED 0 turned ON

geekhouse> sensor read 0
Sensor 0 (light, roof):
  Raw: 2847
  Calibrated: 2847.00 raw
  Timestamp: 12345 ms

geekhouse> sensor calib 0 0.5 100 lux
Sensor 0 calibration updated:
  Formula: y = 0.500 * x + 100.000
  Unit: lux

geekhouse> sensor read 0
Sensor 0 (light, roof):
  Raw: 2847
  Calibrated: 1523.50 lux
  Timestamp: 14567 ms

geekhouse> stats

=== Task States ===
sensor    B  5  3124  2
display   B  4  2456  3
...

geekhouse>
```

### Learning Points

1. **ESP Console Component**:
   - REPL (Read-Eval-Print Loop) pattern
   - Command registration with callbacks
   - Automatic help generation
   - Line editing (arrow keys, backspace work!)

2. **Argument Parsing**:
   - Simple: `argc`/`argv` like standard C
   - Advanced: Use argtable3 for complex commands
   - Always validate user input!

3. **Command Structure**:
   - Each command is a function returning int
   - Return 0 for success, non-zero for error
   - Use printf() for output (not ESP_LOGI in commands)

4. **Real-World Uses**:
   - Manufacturing test sequences
   - Field debugging without reflash
   - Calibration during installation
   - Remote control over network (adapt console to TCP)

### Extension Ideas

- Add command history (built into console component)
- Add tab completion for commands
- Create macro/script support (run multiple commands)
- Add authentication (password protect dangerous commands)
- Export console over telnet/SSH for remote access

---

## Task 5: Watchdog Integration (Medium-Hard - 45 mins)

### Learning Goal
Add task watchdog to detect stuck tasks. Learn how to make systems robust and self-recovering.

### What to Build
Enable FreeRTOS task watchdog and integrate it into all tasks. Intentionally create stuck task scenarios to see detection and recovery.

### Why This Matters
- **Robustness**: Detect when tasks get stuck in infinite loops or deadlocks
- **Production**: Essential for deployed devices that need to recover automatically
- **Debugging**: Helps find bugs that cause tasks to hang
- **Safety**: Critical for safety-critical systems (medical, automotive, industrial)

### Watchdog Concepts

**Watchdog Timer**: A hardware timer that resets the system if not "fed" periodically.

**Task Watchdog**: FreeRTOS watchdog that monitors individual tasks:
- Each task must "reset" (feed) the watchdog periodically
- If a task doesn't reset within timeout, watchdog triggers
- Can print task info, panic, or reset system

**Types**:
- **TWDT (Task Watchdog Timer)**: Monitors FreeRTOS tasks
- **IWDT (Interrupt Watchdog Timer)**: Monitors interrupt handling
- **MWDT (Main Watchdog Timer)**: Hardware watchdog for entire system

### Implementation Steps

#### Step 1: Enable Watchdog in menuconfig

```bash
idf.py menuconfig
```

Navigate to:
1. `Component config` → `ESP System Settings` → `Task Watchdog`
2. Enable: `Initialize Task Watchdog Timer on startup`
3. Set `Task Watchdog timeout period (seconds)`: 10
4. Enable: `Watch CPU0 Idle Task`
5. Enable: `Invoke panic handler on Task Watchdog timeout`

Save and exit.

#### Step 2: Update sensor_task.c

Add watchdog subscription and resets:

```c
#include "esp_task_wdt.h"

void sensor_task(void *pvParameters)
{
    // ... existing parameter handling ...

    ESP_LOGI(TAG, "Sensor task started");

    // Subscribe this task to the watchdog
    // The task MUST reset the watchdog within the timeout period
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));  // NULL = current task
    ESP_LOGI(TAG, "Subscribed to task watchdog");

    while (1) {
        // Read light sensor
        if (sensor_read(SENSOR_LIGHT_ROOF, &reading) == ESP_OK) {
            // ... existing code ...
        }

        // Read water sensor
        if (sensor_read(SENSOR_WATER_ROOF, &reading) == ESP_OK) {
            // ... existing code ...
        }

        // Reset the watchdog - tell it we're still alive
        esp_task_wdt_reset();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    // Cleanup (never reached)
    esp_task_wdt_delete(NULL);
}
```

#### Step 3: Update display_task.c

```c
#include "esp_task_wdt.h"

void display_task(void *pvParameters)
{
    // ... existing parameter handling ...

    ESP_LOGI(TAG, "Display task started");

    // Subscribe to watchdog
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "Subscribed to task watchdog");

    while (1) {
        // Wait for queue item
        if (xQueueReceive(queue, &reading, portMAX_DELAY) == pdTRUE) {
            // ... existing printing code ...

            // Reset watchdog after processing
            esp_task_wdt_reset();
        }
    }

    esp_task_wdt_delete(NULL);
}
```

#### Step 4: Update Other Tasks

Repeat for `reporter_task.c` and `stats_task.c`:

```c
#include "esp_task_wdt.h"

// In task function:
ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

// In main loop, after doing work:
esp_task_wdt_reset();

// Before task exit (if ever):
esp_task_wdt_delete(NULL);
```

#### Step 5: Create Watchdog Test Command

Add to `console_task.c`:

```c
// Watchdog test command - intentionally hang a task
static int cmd_wdt_test(int argc, char **argv)
{
    printf("WARNING: This will trigger the watchdog in 10 seconds!\n");
    printf("The system will panic and show task information.\n");
    printf("Press Ctrl+C to abort...\n");

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("Entering infinite loop (not resetting watchdog)...\n");

    // Infinite busy loop - doesn't yield, doesn't reset watchdog
    while (1) {
        // Do nothing - watchdog will trigger
    }

    return 0;
}

// Register in console_task():
const esp_console_cmd_t wdt_cmd = {
    .command = "wdt-test",
    .help = "Test watchdog (causes panic)",
    .hint = NULL,
    .func = &cmd_wdt_test,
};
esp_console_cmd_register(&wdt_cmd);
```

#### Step 6: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Normal Operation

With watchdog enabled, you should see:
```
I (1234) SENSOR_TASK: Subscribed to task watchdog
I (1234) DISPLAY_TASK: Subscribed to task watchdog
I (1234) REPORTER: Subscribed to task watchdog
I (1234) STATS_TASK: Subscribed to task watchdog
```

Tasks run normally, each resetting watchdog periodically.

### Testing Watchdog Trigger

Type in console:
```
geekhouse> wdt-test
WARNING: This will trigger the watchdog in 10 seconds!
Entering infinite loop (not resetting watchdog)...
```

After ~10 seconds:
```
E (12345) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (12345) task_wdt:  - console (CPU 0)
E (12345) task_wdt: Tasks currently running:
E (12345) task_wdt: CPU 0: console
E (12345) task_wdt: Print CPU 0 (current core) registers
E (12345) task_wdt: Core  0 register dump:
PC      : 0x42008a1e  PS      : 0x00060020  A0      : 0x82008b3c  A1      : 0x3fc91d40
...
[System will panic and reboot]
```

### Learning Points

1. **Subscription Model**:
   - Only subscribed tasks are monitored
   - IDLE task is special - usually auto-subscribed
   - Unsubscribe with `esp_task_wdt_delete()`

2. **Reset Placement**:
   - Reset watchdog after completing work cycle
   - Don't reset in tight loops (defeats purpose!)
   - Balance: frequent enough to avoid false positives, rare enough to catch hangs

3. **Timeout Tuning**:
   - Too short: False positives when task legitimately busy
   - Too long: System hangs for long time before detection
   - Typical: 5-30 seconds depending on task periods

4. **Production Strategy**:
   - Enable watchdog in release builds
   - Configure action: print info, then reboot
   - Log watchdog events to persistent storage
   - Alert if watchdog triggers repeatedly (indicates bug)

5. **Common Causes**:
   - Infinite loops without yields
   - Deadlocks (waiting for mutex that never releases)
   - Blocking on queue/semaphore with no timeout
   - Interrupt-disabled sections too long

### Extension Ideas

- Add watchdog status to stats command
- Create custom panic handler to log to flash before reboot
- Implement escalation: first warning, then reboot on second trigger
- Add external hardware watchdog for ultimate reliability
- Experiment with different timeout values per task

---

## Summary and Next Steps

Congratulations on completing Phase 2! You now have a solid understanding of:

- ✅ FreeRTOS tasks and priorities
- ✅ Queue-based inter-task communication
- ✅ Software timers for periodic operations
- ✅ Event groups for synchronization
- ✅ Task monitoring and profiling
- ✅ Interactive console control
- ✅ Watchdog timers for robustness

### Recommended Learning Path

1. **Start with Task 1** (Stats Monitor) - Quick win, immediately useful
2. **Do Task 2** (Software Timers) - Learn cleaner patterns
3. **Try Task 3** (Event Groups) - Important synchronization primitive
4. **Choose Task 4 or 5** based on interest:
   - Task 4 (Console) if you want practical control interface
   - Task 5 (Watchdog) if you care about system reliability

### Ready for Phase 3?

Once you've completed 2-3 of these tasks, you'll be ready to move to Phase 3:
- WiFi connectivity
- HTTP REST API with HATEOAS
- JSON response generation with cJSON
- NVS configuration storage
- Remote control over network

### Additional Resources

- **FreeRTOS Documentation**: https://www.freertos.org/a00106.html
- **ESP-IDF API Reference**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/
- **Task Watchdog Guide**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/wdts.html
- **Console Component**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/console.html

Happy coding! 🚀
