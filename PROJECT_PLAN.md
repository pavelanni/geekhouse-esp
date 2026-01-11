# Geekhouse ESP-IDF project plan

This document outlines the plan to port the Geekhouse IoT project from MicroPython to ESP-IDF with FreeRTOS.

## Project goal

Recreate the Geekhouse HATEOAS-compliant IoT server using ESP-IDF and FreeRTOS, adding MQTT support for real-time data streaming.

## Hardware setup

### Board

- ESP32-C3 Super Mini on expansion board
- Connected via USB-C

### Current GPIO assignments

| Type   | ID | GPIO | Parameters      |
|--------|---:|-----:|-----------------|
| LED    |  1 |    2 | yellow, roof    |
| LED    |  2 |    3 | white, garden   |
| Sensor |  1 |    1 | water, roof     |
| Sensor |  2 |    0 | light, roof     |

### ESP32-C3 notes

- ADC pins: GPIO0-4 (ADC1 channels)
- GPIO8: Onboard LED (do not use for external devices)
- GPIO9: Boot button (avoid)
- GPIO18/19: USB Serial/JTAG
- All GPIOs support PWM via LEDC driver

## Development environment

- ESP-IDF v6.1 (dev branch)
- VS Code with clangd for code completion
- Build/flash via `idf.py` in terminal
- Console configured for USB Serial/JTAG

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     app_main()                          │
│         (Initialize NVS, WiFi, create tasks)            │
└─────────────────────────────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┬─────────────────┐
        ▼                 ▼                 ▼                 ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  WiFi Task   │  │ Sensor Task  │  │  HTTP Task   │  │  MQTT Task   │
│  (connect,   │  │ (read ADC,   │  │  (REST API,  │  │  (publish    │
│   reconnect) │  │  GPIO, push  │  │   HATEOAS)   │  │   readings,  │
└──────────────┘  │  to queues)  │  └──────────────┘  │   subscribe) │
                  └──────────────┘                    └──────────────┘
                          │                                   ▲
                          └───────────────────────────────────┘
                                    (via queue)
```

### FreeRTOS primitives to use

- **Tasks**: Separate tasks for sensors, HTTP server, MQTT client
- **Queues**: Pass sensor readings between tasks
- **Event groups**: Coordinate startup (WiFi connected, MQTT connected)
- **Mutexes**: Protect shared device state
- **Software timers**: Periodic sensor readings

## Implementation phases

### Phase 1: Project setup and basic I/O ✓ (partially done)

- [x] ESP-IDF installation on Fedora
- [x] VS Code setup with clangd
- [x] Basic FreeRTOS concepts (tasks, queues, event groups)
- [x] WiFi connection
- [x] HTTP server with `/status` endpoint
- [x] NVS for WiFi credentials
- [x] Console for debugging
- [ ] Test LED control on GPIO2 and GPIO3
- [ ] Test ADC reading on GPIO0 and GPIO1

### Phase 2: Sensor and actuator drivers

- [ ] Create LED driver module
  - Initialize GPIO as output
  - Toggle, on, off functions
  - State tracking
- [ ] Create ADC sensor driver module
  - Initialize ADC for GPIO0, GPIO1
  - Raw reading function
  - Calibration support (linear, polynomial)
- [ ] Create sensor task
  - Periodic reading (configurable interval)
  - Push readings to queue
- [ ] Unit conversion and calibration
  - Light sensor: raw ADC to lux
  - Water sensor: raw ADC to percentage

### Phase 3: HATEOAS REST API

- [ ] Implement cJSON for response building
- [ ] Root endpoint `/` with links
- [ ] LED endpoints
  - `GET /leds` - list all LEDs with state and links
  - `GET /leds/{id}` - single LED details
  - `POST /leds/{id}/on` - turn on
  - `POST /leds/{id}/off` - turn off
  - `POST /leds/{id}/toggle` - toggle state
  - `GET /leds/filter?color={color}&location={location}`
- [ ] Sensor endpoints
  - `GET /sensors` - list all sensors with links
  - `GET /sensors/{id}` - single sensor details
  - `GET /sensors/{id}/value` - current reading (raw + calibrated)
  - `GET /sensors/{id}/config` - calibration config
  - `POST /sensors/{id}/config` - update calibration
  - `GET /sensors/filter?type={type}&location={location}`
- [ ] Status endpoint
  - `GET /status` - system info (uptime, free heap, WiFi RSSI)

### Phase 4: Device configuration system

- [ ] Define configuration structure in C
- [ ] Store device config in NVS or SPIFFS
- [ ] Load config at startup
- [ ] API endpoints to update config
- [ ] Console commands for config management

### Phase 5: MQTT integration

- [ ] Add esp_mqtt component
- [ ] MQTT connection task
- [ ] Publish sensor readings periodically
  - Topic: `geekhouse/sensors/{id}/value`
  - Payload: JSON with timestamp, raw, calibrated values
- [ ] Subscribe to actuator commands
  - Topic: `geekhouse/leds/{id}/command`
  - Payload: `on`, `off`, `toggle`
- [ ] Last Will and Testament for connection status
- [ ] Reconnection handling

### Phase 6: Additional features (optional)

- [ ] mDNS for `geekhouse.local` hostname
- [ ] OTA updates
- [ ] Web UI (serve static files from SPIFFS)
- [ ] Additional sensors (motion, touch, temperature)
- [ ] PWM actuators (servo, motor, buzzer)

## File structure

```
geekhouse-espidf/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                 # app_main, task creation
│   ├── wifi.c / wifi.h        # WiFi connection management
│   ├── http_server.c / .h     # REST API handlers
│   ├── mqtt_client.c / .h     # MQTT pub/sub
│   ├── sensors.c / .h         # Sensor drivers and task
│   ├── actuators.c / .h       # LED and other actuators
│   ├── config.c / .h          # Device configuration
│   ├── calibration.c / .h     # Sensor calibration math
│   └── nvs_storage.c / .h     # NVS wrapper functions
└── components/                 # Optional custom components
```

## Key differences from MicroPython version

| Aspect | MicroPython | ESP-IDF |
|--------|-------------|---------|
| Language | Python | C |
| Web framework | Microdot | esp_http_server |
| Async | asyncio | FreeRTOS tasks |
| Config format | TOML → JSON | NVS or JSON in SPIFFS |
| Memory management | Automatic | Manual (but mostly stack-based) |
| JSON handling | Built-in dict | cJSON library |

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/)
- [FreeRTOS Book](https://www.freertos.org/Documentation/02-Kernel/07-Books-and-manual/01-RTOS_book)
- [ESP-IDF HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)
- [ESP-IDF MQTT](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html)
- [cJSON Library](https://github.com/DaveGamble/cJSON)
- [Original Geekhouse repo](https://github.com/pavelanni/geekhouse)

## Notes from learning sessions

### FreeRTOS concepts learned

- Tasks are like goroutines but heavier (explicit stack allocation)
- Queues are like Go channels (pass by copy, not reference)
- Event groups for waiting on multiple conditions (AND/OR)
- `vTaskDelay(pdMS_TO_TICKS(ms))` for non-blocking delays

### ESP-IDF patterns

- `ESP_ERROR_CHECK()` for fatal errors during init
- `ESP_LOGI/W/E()` for logging with tags
- Component dependencies via `PRIV_REQUIRES` in CMakeLists.txt
- NVS for persistent key-value storage
- Event-driven WiFi with callbacks and event groups

### Build commands

```bash
# Source ESP-IDF environment
get-idf  # alias for: source ~/esp/esp-idf/export.sh

# Build and flash
idf.py build
idf.py flash monitor

# Full clean (after menuconfig changes)
idf.py fullclean

# Configuration menu
idf.py menuconfig
```

### VS Code setup

- clangd extension for code completion
- `compile_commands.json` generated by build
- Console configured for USB Serial/JTAG in menuconfig
- Use picocom for interactive console: `picocom /dev/ttyACM0 -b 115200`
