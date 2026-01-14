# Geekhouse ESP

## Project Overview

Geekhouse ESP is a port of the [Geekhouse IoT project](https://github.com/pavelanni/geekhouse) to the ESP32-C3 platform using the **ESP-IDF** framework and **FreeRTOS**.

The goal is to create a **HATEOAS-compliant IoT server** that exposes sensors and actuators via a REST API and supports real-time data streaming over MQTT.

## Technical Stack

*   **Hardware:** ESP32-C3 Super Mini
*   **Language:** C
*   **Framework:** ESP-IDF (v6.1 dev)
*   **OS:** FreeRTOS
*   **Build System:** CMake / `idf.py`

## Architecture

The application is built around a multi-tasking architecture using FreeRTOS primitives:

*   **`app_main`**: Initializes drivers (NVS, WiFi, etc.) and spawns the application tasks.
*   **Tasks**:
    *   `sensor_task`: Periodically reads sensor data (ADC) and pushes it to a queue.
    *   `display_task`: Consumes sensor data from the queue and handles display/logging.
    *   `led_task`: Manages LED states and blinking patterns independently.
    *   `stats_task`: Monitors and logs system health (heap usage, stack high water mark).
*   **IPC**: Uses FreeRTOS queues for thread-safe data transfer between acquisition (sensor) and presentation (display/network) tasks.

## Hardware Configuration (ESP32-C3)

| Component | ID  | GPIO | Details       |
| :---      | :-- | :--- | :---          |
| **LED**   | 1   | 2    | Yellow (Roof) |
| **LED**   | 2   | 3    | White (Garden)|
| **Sensor**| 1   | 1    | Water (Roof)  |
| **Sensor**| 2   | 0    | Light (Roof)  |

**Note:**
*   GPIO8 is the onboard LED (avoid using).
*   GPIO18/19 are used for USB Serial/JTAG.

## Current Status

The project is currently in **Phase 2: Multi-Task Architecture**.
*   Basic FreeRTOS task and queue structure is implemented.
*   Sensor and Actuator drivers are initialized.
*   The system reads sensors, updates LEDs, and reports statistics.

## Development Commands

*   **Build:** `idf.py build`
*   **Flash:** `idf.py flash`
*   **Monitor:** `idf.py monitor`
*   **Clean:** `idf.py fullclean`
*   **Menuconfig:** `idf.py menuconfig`

## Key Documentation
*   `PROJECT_PLAN.md`: Detailed roadmap and implementation phases.
*   `README.md`: General project information.
