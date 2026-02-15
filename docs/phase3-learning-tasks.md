# Phase 3 Learning Tasks - Network Connectivity

This document contains hands-on learning tasks to add network connectivity to your Geekhouse ESP32-C3. You'll connect to WiFi, expose sensor data via a REST API, and publish readings over MQTT.

**Prerequisites:** Completed Phase 2 (FreeRTOS tasks, queues, event groups, shared data). You should have a working system with sensor and display tasks communicating via queues.

**Recommended approach:** Complete tasks in order. Each task builds on the previous one.

---

## Task 1: NVS Configuration Storage (Easy - 30 mins)

### Learning Goal

Use Non-Volatile Storage (NVS) to persist WiFi credentials across reboots. Learn how Kconfig provides compile-time defaults that NVS can override at runtime.

### What to Build

A configuration module that:

- Stores WiFi SSID and password in NVS flash
- Uses Kconfig menuconfig for default values
- Allows runtime updates (persisted across reboots)

### Why This Matters

- **Security**: Credentials stored in flash, not hardcoded in source
- **Flexibility**: Change WiFi network without reflashing
- **Production**: Factory defaults via Kconfig, site-specific config via NVS
- **Pattern**: Same approach works for any persistent configuration

### NVS Concepts

**NVS (Non-Volatile Storage)** is a key-value store in flash memory:

- Survives reboots and power cycles
- Organized into **namespaces** (like folders)
- Supports strings, integers, blobs
- Wear-leveled (flash-friendly)

**Kconfig** is ESP-IDF's build-time configuration system:

- Defines defaults via `Kconfig.projbuild`
- Values accessible as `CONFIG_*` macros
- Set via `idf.py menuconfig`
- Compiled into the binary (not changeable at runtime)

**The pattern**: Kconfig provides factory defaults. On first boot, store them in NVS. After that, NVS values take priority (can be updated at runtime).

### Implementation Steps

#### Step 1: Create Kconfig.projbuild

Create `main/Kconfig.projbuild`:

```
menu "Geekhouse Configuration"

    config GEEKHOUSE_WIFI_SSID
        string "WiFi SSID"
        default "myssid"
        help
            Default WiFi network name. This is used as the initial value
            stored in NVS on first boot. Can be changed at runtime.

    config GEEKHOUSE_WIFI_PASSWORD
        string "WiFi Password"
        default "mypassword"
        help
            Default WiFi password. This is used as the initial value
            stored in NVS on first boot. Can be changed at runtime.

endmenu
```

#### Step 2: Create wifi_config.h

```c
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_err.h"

// Maximum lengths (WiFi spec limits)
#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASSWORD_MAX_LEN 64

/**
 * Initialize WiFi configuration
 *
 * Opens NVS namespace "wifi_config". If no credentials are stored,
 * writes the Kconfig defaults (CONFIG_GEEKHOUSE_WIFI_SSID/PASSWORD).
 *
 * Must be called after nvs_flash_init().
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_config_init(void);

/**
 * Get stored WiFi SSID
 *
 * @param[out] ssid Buffer to receive SSID (must be >= WIFI_SSID_MAX_LEN + 1)
 * @param len Buffer length
 * @return ESP_OK on success
 */
esp_err_t wifi_config_get_ssid(char *ssid, size_t len);

/**
 * Get stored WiFi password
 *
 * @param[out] password Buffer to receive password (must be >= WIFI_PASSWORD_MAX_LEN + 1)
 * @param len Buffer length
 * @return ESP_OK on success
 */
esp_err_t wifi_config_get_password(char *password, size_t len);

/**
 * Update WiFi credentials in NVS
 *
 * New values persist across reboots. Takes effect on next WiFi connection.
 *
 * @param ssid New SSID
 * @param password New password
 * @return ESP_OK on success
 */
esp_err_t wifi_config_set_credentials(const char *ssid, const char *password);

#endif // WIFI_CONFIG_H
```

#### Step 3: Create wifi_config.c

```c
#include "wifi_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI_CONFIG";

// NVS namespace and keys
#define NVS_NAMESPACE "wifi_config"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "password"

static nvs_handle_t s_nvs_handle;

esp_err_t wifi_config_init(void)
{
    esp_err_t ret;

    // Open NVS namespace
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    // Check if SSID exists in NVS
    size_t required_size = 0;
    ret = nvs_get_str(s_nvs_handle, NVS_KEY_SSID, NULL, &required_size);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // First boot: store Kconfig defaults into NVS
        ESP_LOGI(TAG, "No WiFi credentials in NVS, storing defaults from Kconfig");

        ret = nvs_set_str(s_nvs_handle, NVS_KEY_SSID, CONFIG_GEEKHOUSE_WIFI_SSID);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to store default SSID: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_set_str(s_nvs_handle, NVS_KEY_PASS, CONFIG_GEEKHOUSE_WIFI_PASSWORD);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to store default password: %s", esp_err_to_name(ret));
            return ret;
        }

        // Commit writes to flash
        ret = nvs_commit(s_nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "Default credentials stored (SSID: %s)", CONFIG_GEEKHOUSE_WIFI_SSID);
    } else if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials found in NVS");
    } else {
        ESP_LOGE(TAG, "Error reading NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_config_get_ssid(char *ssid, size_t len)
{
    return nvs_get_str(s_nvs_handle, NVS_KEY_SSID, ssid, &len);
}

esp_err_t wifi_config_get_password(char *password, size_t len)
{
    return nvs_get_str(s_nvs_handle, NVS_KEY_PASS, password, &len);
}

esp_err_t wifi_config_set_credentials(const char *ssid, const char *password)
{
    esp_err_t ret;

    ret = nvs_set_str(s_nvs_handle, NVS_KEY_SSID, ssid);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_str(s_nvs_handle, NVS_KEY_PASS, password);
    if (ret != ESP_OK) return ret;

    ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Credentials updated (SSID: %s)", ssid);
    return ESP_OK;
}
```

#### Step 4: Update main.c

```c
#include "nvs_flash.h"
#include "wifi_config.h"

void app_main(void)
{
    // Initialize NVS (must be before wifi_config_init)
    ESP_LOGI(TAG, "Initializing NVS flash...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was corrupted or wrong version - erase and retry
        ESP_LOGW(TAG, "NVS corrupted, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi configuration from NVS
    ESP_LOGI(TAG, "Initializing WiFi configuration...");
    ESP_ERROR_CHECK(wifi_config_init());

    // Print current credentials
    char ssid[WIFI_SSID_MAX_LEN + 1];
    wifi_config_get_ssid(ssid, sizeof(ssid));
    ESP_LOGI(TAG, "Configured WiFi SSID: %s", ssid);

    // ... rest of existing initialization ...
}
```

#### Step 5: Update CMakeLists.txt

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
        "wifi_config.c"        # Add this
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash    # Add this
)
```

#### Step 6: Set Your WiFi Credentials

```bash
idf.py menuconfig
```

Navigate to `Geekhouse Configuration` and enter your WiFi SSID and password.

#### Step 7: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Output

First boot:

```none
I (345) WIFI_CONFIG: No WiFi credentials in NVS, storing defaults from Kconfig
I (355) WIFI_CONFIG: Default credentials stored (SSID: MyHomeWiFi)
I (365) main: Configured WiFi SSID: MyHomeWiFi
```

Subsequent boots:

```none
I (345) WIFI_CONFIG: WiFi credentials found in NVS
I (355) main: Configured WiFi SSID: MyHomeWiFi
```

### Learning Points

1. **NVS API Pattern**:
   - `nvs_open()` - Open namespace (like opening a file)
   - `nvs_get_str()` / `nvs_set_str()` - Read/write strings
   - `nvs_commit()` - Flush writes to flash (important!)
   - `nvs_close()` - Close handle (optional for long-lived handles)

2. **Kconfig Integration**:
   - `Kconfig.projbuild` in main/ is auto-discovered by build system
   - Values become `CONFIG_*` preprocessor macros
   - Stored in `sdkconfig` file (don't commit credentials to git!)

3. **Error Handling**:
   - `ESP_ERR_NVS_NOT_FOUND` - Key doesn't exist yet (first boot)
   - `ESP_ERR_NVS_NO_FREE_PAGES` - Flash corrupted, needs erase
   - Always handle the erase-and-retry pattern for NVS init

4. **Security Notes**:
   - Add `sdkconfig` to `.gitignore` to avoid committing credentials
   - NVS is not encrypted by default (use NVS encryption for production)
   - Kconfig defaults are compiled into the binary

### Extension Ideas

- Add a console command to change WiFi credentials at runtime
- Store additional config (MQTT broker address, device name, update interval)
- Add NVS encryption for credential protection
- Implement factory reset (erase NVS namespace)

---

## Task 2: WiFi Station Connection (Medium - 45 mins)

### Learning Goal

Connect the ESP32-C3 to a WiFi network using the ESP-IDF WiFi driver. Learn the WiFi event system and use event groups (from Phase 2) to signal connection status.

### What to Build

A WiFi manager that:

- Connects to WiFi using credentials from NVS (Task 1)
- Uses an event group to signal connected/disconnected status
- Auto-reconnects on disconnect with retry limit
- Logs IP address on successful connection

### Why This Matters

- **Foundation**: WiFi is required for HTTP and MQTT
- **Event System**: ESP-IDF events are used throughout (WiFi, IP, MQTT, HTTP)
- **Reliability**: Auto-reconnect is essential for production devices
- **Reuse**: Event group pattern from Phase 2 applies directly

### WiFi Station Concepts

**Station Mode (STA)**: ESP32 connects to an existing WiFi network (like a phone connecting to your router).

**ESP-IDF Event Loop**: The WiFi driver communicates via events:

1. `WIFI_EVENT_STA_START` - WiFi driver started, ready to connect
2. `WIFI_EVENT_STA_CONNECTED` - Associated with AP (not yet IP)
3. `WIFI_EVENT_STA_DISCONNECTED` - Lost connection
4. `IP_EVENT_STA_GOT_IP` - DHCP assigned an IP address (fully connected!)

**Connection sequence**:

```text
esp_wifi_start() → STA_START event → esp_wifi_connect()
    → STA_CONNECTED event → DHCP negotiation → GOT_IP event
```

### Implementation Steps

#### Step 1: Create wifi_manager.h

```c
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event group bits for WiFi status
#define WIFI_CONNECTED_BIT    BIT0  // Connected and got IP
#define WIFI_DISCONNECTED_BIT BIT1  // Disconnected or failed

// Maximum connection retries before giving up
#define WIFI_MAX_RETRIES 10

/**
 * Initialize and start WiFi in station mode
 *
 * Reads credentials from NVS (wifi_config module), initializes
 * the WiFi driver, and begins connection attempts.
 *
 * Must be called after wifi_config_init().
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * Get WiFi event group
 *
 * Use this to wait for connection status in other tasks:
 *
 *   EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
 *   xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT,
 *                       pdFALSE, pdTRUE, portMAX_DELAY);
 *
 * @return Event group handle
 */
EventGroupHandle_t wifi_manager_get_event_group(void);

/**
 * Check if WiFi is currently connected
 *
 * @return true if connected and has IP address
 */
bool wifi_manager_is_connected(void);

#endif // WIFI_MANAGER_H
```

#### Step 2: Create wifi_manager.c

```c
#include "wifi_manager.h"
#include "wifi_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;

/**
 * WiFi and IP event handler
 *
 * This function is called by the ESP-IDF event loop when WiFi
 * or IP events occur. It runs in the event task context.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                // WiFi driver started - initiate connection
                ESP_LOGI(TAG, "WiFi started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                // Associated with AP, waiting for IP
                ESP_LOGI(TAG, "Connected to AP, waiting for IP...");
                s_retry_count = 0;
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                // Lost connection - attempt reconnect
                wifi_event_sta_disconnected_t *event =
                    (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "Disconnected (reason: %d)", event->reason);

                // Signal disconnected
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);

                if (s_retry_count < WIFI_MAX_RETRIES) {
                    s_retry_count++;
                    ESP_LOGI(TAG, "Reconnecting (attempt %d/%d)...",
                             s_retry_count, WIFI_MAX_RETRIES);
                    esp_wifi_connect();
                } else {
                    ESP_LOGE(TAG, "Max retries reached, giving up");
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // Got IP address - fully connected!
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
            ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&event->ip_info.netmask));

            // Signal connected
            xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret;

    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station network interface
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, NULL));

    // Load credentials from NVS
    char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
    char password[WIFI_PASSWORD_MAX_LEN + 1] = {0};

    ret = wifi_config_get_ssid(ssid, sizeof(ssid));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SSID from NVS");
        return ret;
    }

    ret = wifi_config_get_password(password, sizeof(password));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get password from NVS");
        return ret;
    }

    // Configure WiFi station
    wifi_config_t wifi_cfg = {
        .sta = {
            // SSID and password will be copied below
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Copy credentials (strncpy for safety)
    strncpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    // Start WiFi (triggers WIFI_EVENT_STA_START → connect)
    ESP_LOGI(TAG, "Starting WiFi (SSID: %s)...", ssid);
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

EventGroupHandle_t wifi_manager_get_event_group(void)
{
    return s_wifi_event_group;
}

bool wifi_manager_is_connected(void)
{
    if (s_wifi_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
```

#### Step 3: Update main.c

```c
#include "wifi_manager.h"

void app_main(void)
{
    // ... NVS init and wifi_config_init() from Task 1 ...

    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(wifi_manager_init());

    // Wait for WiFi connection before starting network services
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    EventGroupHandle_t wifi_events = wifi_manager_get_event_group();
    EventBits_t bits = xEventGroupWaitBits(
        wifi_events,
        WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
        pdFALSE,       // Don't clear bits
        pdFALSE,       // Wait for ANY bit (OR)
        pdMS_TO_TICKS(30000)  // 30 second timeout
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected!");
    } else {
        ESP_LOGW(TAG, "WiFi connection timed out, continuing without network");
    }

    // ... rest of task creation ...
}
```

#### Step 4: Update CMakeLists.txt

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
        "wifi_config.c"
        "wifi_manager.c"       # Add this
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif  # Add esp_wifi esp_netif
)
```

#### Step 5: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Expected Output

```none
I (456) WIFI_MGR: Starting WiFi (SSID: MyHomeWiFi)...
I (478) wifi:mode : sta (aa:bb:cc:dd:ee:ff)
I (480) WIFI_MGR: WiFi started, connecting...
I (1234) wifi:connected to ap SSID:MyHomeWiFi
I (1234) WIFI_MGR: Connected to AP, waiting for IP...
I (2456) WIFI_MGR: Got IP address: 192.168.1.42
I (2456) WIFI_MGR: Gateway: 192.168.1.1
I (2456) WIFI_MGR: Netmask: 255.255.255.0
I (2456) main: WiFi connected!
```

If WiFi is unavailable:

```none
I (1234) WIFI_MGR: WiFi started, connecting...
W (3456) WIFI_MGR: Disconnected (reason: 201)
I (3456) WIFI_MGR: Reconnecting (attempt 1/10)...
W (5678) WIFI_MGR: Disconnected (reason: 201)
I (5678) WIFI_MGR: Reconnecting (attempt 2/10)...
...
```

### Testing

1. **Happy path**: Set correct credentials, verify IP appears
2. **Wrong password**: Set wrong password in menuconfig, observe retry/fail behavior
3. **Router reboot**: Unplug router, watch disconnect events, plug back in, verify reconnect
4. **Check IP**: From another device on same network, `ping 192.168.1.42`

### Learning Points

1. **ESP-IDF Event System**:
   - Events are dispatched through the default event loop
   - Register handlers with `esp_event_handler_instance_register()`
   - Handler runs in the event task (not your task) - keep it fast
   - Same pattern used for WiFi, IP, MQTT, HTTP server, etc.

2. **WiFi Initialization Sequence**:

   ```text
   esp_netif_init() → event_loop_create_default() → esp_netif_create_default_wifi_sta()
       → esp_wifi_init() → register handlers → esp_wifi_set_config() → esp_wifi_start()
   ```

   This sequence is always the same. Memorize it or keep it as a template.

3. **Event Group Reuse**:
   - Same pattern from Phase 2 Task 3 (reporter_task)
   - WiFi events set/clear bits
   - Other tasks wait on bits to know when network is available
   - Clean separation: wifi_manager owns the event group, others just read it

4. **Disconnect Reasons**:
   - 201: AP not found (wrong SSID, AP offline)
   - 2: Authentication failed (wrong password)
   - 8: AP disconnected (router rebooted)
   - Full list in `esp_wifi_types.h`

### Extension Ideas

- Add LED indicator for WiFi status (blink while connecting, solid when connected)
- Display IP address on the stats printout
- Add mDNS so you can reach the device as `geekhouse.local`
- Support WPA3 authentication
- Add a console command to trigger reconnect

---

## Task 3: HTTP REST API with JSON (Medium-Hard - 60 mins)

### Learning Goal

Create an HTTP server that exposes sensor data and LED control as a REST API with JSON responses. Learn ESP-IDF's HTTP server component and cJSON for JSON serialization.

### What to Build

A REST API with these endpoints:

| Method | Endpoint            | Description                            |
| ------ | ------------------- | -------------------------------------- |
| GET    | `/api/sensors`      | List all sensors with current readings |
| GET    | `/api/sensors/{id}` | Get single sensor reading              |
| GET    | `/api/leds`         | List all LEDs with current state       |
| POST   | `/api/leds/{id}`    | Control LED (on/off/toggle)            |
| GET    | `/api/system`       | System info (uptime, heap, WiFi)       |

### Why This Matters

- **Remote Monitoring**: Read sensor data from any browser or script
- **Remote Control**: Control LEDs without physical access
- **Integration**: REST APIs connect to dashboards, Home Assistant, etc.
- **Standard**: HTTP + JSON is the universal integration language

### cJSON Overview

**cJSON** is a lightweight JSON library for C:

```c
// Create a JSON object
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "name", "light");
cJSON_AddNumberToObject(root, "value", 2847);

// Convert to string
char *json_str = cJSON_PrintUnformatted(root);
// json_str = {"name":"light","value":2847}

// Clean up
cJSON_Delete(root);   // Free the JSON tree
free(json_str);       // Free the printed string
```

### Implementation Steps

#### Step 1: Add cJSON Dependency

Create `main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: ">=5.0.0"
  espressif/cjson:
    version: "^1.7.18"
```

This tells the ESP-IDF component manager to download cJSON automatically.

#### Step 2: Create http_server.h

```c
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"

/**
 * Start the HTTP REST API server
 *
 * Registers URI handlers and starts listening on port 80.
 * Must be called after WiFi is connected.
 *
 * @return ESP_OK on success
 */
esp_err_t http_server_start(void);

/**
 * Stop the HTTP server
 *
 * @return ESP_OK on success
 */
esp_err_t http_server_stop(void);

#endif // HTTP_SERVER_H
```

#### Step 3: Create http_server.c

```c
#include "http_server.h"
#include "sensors.h"
#include "actuators.h"
#include "sensor_data_shared.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "HTTP_SRV";
static httpd_handle_t s_server = NULL;

/**
 * Helper: Send JSON response
 *
 * Sets Content-Type header and sends the cJSON object as response.
 * Frees the cJSON object after sending.
 */
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *json_str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (json_str == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                           "Failed to generate JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    free(json_str);
    return ESP_OK;
}

/**
 * Helper: Send error JSON response
 */
static esp_err_t send_error_response(httpd_req_t *req, int status,
                                      const char *message)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message);

    httpd_resp_set_status(req, status == 404 ? "404 Not Found" : "400 Bad Request");
    return send_json_response(req, json);
}

// ---- GET /api/sensors ----

static esp_err_t get_sensors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *sensors = cJSON_AddArrayToObject(root, "sensors");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        const sensor_info_t *info = sensor_get_info(i);
        sensor_reading_t reading;
        esp_err_t ret = sensor_read(i, &reading);

        cJSON *sensor = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensor, "id", i);
        cJSON_AddStringToObject(sensor, "type",
            info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
        cJSON_AddStringToObject(sensor, "location", info->location);

        if (ret == ESP_OK) {
            cJSON_AddNumberToObject(sensor, "raw_value", reading.raw_value);
            cJSON_AddNumberToObject(sensor, "calibrated_value",
                                    reading.calibrated_value);
            cJSON_AddStringToObject(sensor, "unit", reading.unit);
            cJSON_AddNumberToObject(sensor, "timestamp", reading.timestamp);
        } else {
            cJSON_AddStringToObject(sensor, "error", "read failed");
        }

        cJSON_AddItemToArray(sensors, sensor);
    }

    return send_json_response(req, root);
}

// ---- GET /api/sensors/{id} ----

static esp_err_t get_sensor_by_id_handler(httpd_req_t *req)
{
    // Extract sensor ID from URI
    // URI is like "/api/sensors/0" - get the last character
    const char *uri = req->uri;
    int id = uri[strlen("/api/sensors/")] - '0';

    if (id < 0 || id >= SENSOR_COUNT) {
        return send_error_response(req, 404, "Sensor not found");
    }

    const sensor_info_t *info = sensor_get_info(id);
    sensor_reading_t reading;
    esp_err_t ret = sensor_read(id, &reading);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "type",
        info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
    cJSON_AddStringToObject(root, "location", info->location);

    if (ret == ESP_OK) {
        cJSON_AddNumberToObject(root, "raw_value", reading.raw_value);
        cJSON_AddNumberToObject(root, "calibrated_value",
                                reading.calibrated_value);
        cJSON_AddStringToObject(root, "unit", reading.unit);
        cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
    }

    return send_json_response(req, root);
}

// ---- GET /api/leds ----

static esp_err_t get_leds_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *leds = cJSON_AddArrayToObject(root, "leds");

    for (int i = 0; i < LED_COUNT; i++) {
        const led_info_t *info = led_get_info(i);
        bool state;
        led_get_state(i, &state);

        cJSON *led = cJSON_CreateObject();
        cJSON_AddNumberToObject(led, "id", i);
        cJSON_AddStringToObject(led, "color", info->color);
        cJSON_AddStringToObject(led, "location", info->location);
        cJSON_AddBoolToObject(led, "state", state);

        cJSON_AddItemToArray(leds, led);
    }

    return send_json_response(req, root);
}

// ---- POST /api/leds/{id} ----
// Body: {"action": "on"} or {"action": "off"} or {"action": "toggle"}

static esp_err_t post_led_handler(httpd_req_t *req)
{
    // Extract LED ID from URI
    const char *uri = req->uri;
    int id = uri[strlen("/api/leds/")] - '0';

    if (id < 0 || id >= LED_COUNT) {
        return send_error_response(req, 404, "LED not found");
    }

    // Read request body
    char body[128] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return send_error_response(req, 400, "Empty request body");
    }

    // Parse JSON body
    cJSON *json = cJSON_Parse(body);
    if (json == NULL) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    cJSON *action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action)) {
        cJSON_Delete(json);
        return send_error_response(req, 400,
            "Missing 'action' field (on/off/toggle)");
    }

    // Execute action
    esp_err_t ret;
    if (strcmp(action->valuestring, "on") == 0) {
        ret = led_on(id);
    } else if (strcmp(action->valuestring, "off") == 0) {
        ret = led_off(id);
    } else if (strcmp(action->valuestring, "toggle") == 0) {
        ret = led_toggle(id);
    } else {
        cJSON_Delete(json);
        return send_error_response(req, 400,
            "Invalid action (use: on, off, toggle)");
    }

    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return send_error_response(req, 400, "LED operation failed");
    }

    // Return updated LED state
    bool state;
    led_get_state(id, &state);
    const led_info_t *info = led_get_info(id);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "color", info->color);
    cJSON_AddStringToObject(root, "location", info->location);
    cJSON_AddBoolToObject(root, "state", state);

    return send_json_response(req, root);
}

// ---- GET /api/system ----

static esp_err_t get_system_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    // Uptime
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    cJSON_AddNumberToObject(root, "uptime_ms", (double)uptime_ms);

    // Memory
    cJSON *memory = cJSON_AddObjectToObject(root, "memory");
    cJSON_AddNumberToObject(memory, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(memory, "min_free_heap",
                            esp_get_minimum_free_heap_size());

    // WiFi (basic info)
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
        cJSON_AddStringToObject(wifi, "ssid", (char *)ap_info.ssid);
        cJSON_AddNumberToObject(wifi, "rssi", ap_info.rssi);
        cJSON_AddNumberToObject(wifi, "channel", ap_info.primary);
    }

    return send_json_response(req, root);
}

// ---- URI registration ----

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    const httpd_uri_t uris[] = {
        {
            .uri = "/api/sensors",
            .method = HTTP_GET,
            .handler = get_sensors_handler,
        },
        {
            .uri = "/api/sensors/*",
            .method = HTTP_GET,
            .handler = get_sensor_by_id_handler,
        },
        {
            .uri = "/api/leds",
            .method = HTTP_GET,
            .handler = get_leds_handler,
        },
        {
            .uri = "/api/leds/*",
            .method = HTTP_POST,
            .handler = post_led_handler,
        },
        {
            .uri = "/api/system",
            .method = HTTP_GET,
            .handler = get_system_handler,
        },
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started with %d endpoints",
             (int)(sizeof(uris) / sizeof(uris[0])));
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}
```

#### Step 4: Update main.c

Start the HTTP server after WiFi connects:

```c
#include "http_server.h"

void app_main(void)
{
    // ... existing init (NVS, wifi_config, wifi_manager, wait for connection) ...

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected!");

        // Start HTTP server
        ESP_LOGI(TAG, "Starting HTTP server...");
        ESP_ERROR_CHECK(http_server_start());
    } else {
        ESP_LOGW(TAG, "WiFi connection timed out, HTTP server not started");
    }

    // ... rest of task creation ...
}
```

#### Step 5: Update CMakeLists.txt

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
        "wifi_config.c"
        "wifi_manager.c"
        "http_server.c"        # Add this
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_http_server  # Add esp_http_server
)
```

#### Step 6: Build and Test

```bash
idf.py build
idf.py flash monitor
```

Note the IP address from the WiFi log, then test from your computer.

### Testing with curl

Replace `192.168.1.42` with your ESP32's IP address.

```bash
# Get all sensors
curl http://192.168.1.42/api/sensors | jq .

# Get single sensor
curl http://192.168.1.42/api/sensors/0 | jq .

# Get all LEDs
curl http://192.168.1.42/api/leds | jq .

# Turn LED on
curl -X POST http://192.168.1.42/api/leds/0 \
     -H "Content-Type: application/json" \
     -d '{"action": "on"}' | jq .

# Toggle LED
curl -X POST http://192.168.1.42/api/leds/1 \
     -H "Content-Type: application/json" \
     -d '{"action": "toggle"}' | jq .

# Get system info
curl http://192.168.1.42/api/system | jq .
```

### Expected Responses

**GET /api/sensors**:

```json
{
  "sensors": [
    {
      "id": 0,
      "type": "light",
      "location": "roof",
      "raw_value": 2847,
      "calibrated_value": 2847.0,
      "unit": "raw",
      "timestamp": 45678
    },
    {
      "id": 1,
      "type": "water",
      "location": "roof",
      "raw_value": 1534,
      "calibrated_value": 1534.0,
      "unit": "raw",
      "timestamp": 45680
    }
  ]
}
```

**POST /api/leds/0** with `{"action": "on"}`:

```json
{
  "id": 0,
  "color": "yellow",
  "location": "roof",
  "state": true
}
```

**GET /api/system**:

```json
{
  "uptime_ms": 123456,
  "memory": {
    "free_heap": 245632,
    "min_free_heap": 243104
  },
  "wifi": {
    "ssid": "MyHomeWiFi",
    "rssi": -42,
    "channel": 6
  }
}
```

### Learning Points

1. **ESP HTTP Server**:
   - Runs in its own task (created by `httpd_start()`)
   - Each handler runs in the server's task context
   - Wildcard matching with `httpd_uri_match_wildcard`
   - Max connections: 7 by default (configurable)

2. **cJSON Memory Management**:
   - `cJSON_CreateObject()` allocates from heap
   - `cJSON_Delete()` frees the entire tree
   - `cJSON_PrintUnformatted()` allocates a new string (must `free()` it)
   - Always check for NULL returns
   - Use `cJSON_PrintUnformatted()` for smaller responses (no whitespace)

3. **Thread Safety**:
   - HTTP handlers run in the httpd task, not your sensor task
   - Reading sensors directly from handlers is OK (sensor driver has internal mutex)
   - Accessing shared data structures needs the mutex from Phase 2
   - Each request is handled sequentially (no concurrent handler execution)

4. **REST Conventions**:
   - GET for reading data (idempotent, safe)
   - POST for actions/mutations
   - Return updated state after mutation
   - Use proper HTTP status codes (200, 400, 404)

### Extension Ideas

- Add CORS headers for browser access
- Serve a simple HTML dashboard at `/`
- Add query parameters for filtering (e.g., `?type=light`)
- Support PUT for updating sensor calibration
- Add request logging middleware

---

## Task 4: HATEOAS Enhancement (Medium - 30 mins)

### Learning Goal

Add hypermedia links (HATEOAS) to your REST API responses, making the API self-documenting and discoverable. Learn why HATEOAS matters for IoT device APIs.

### What to Build

Modify the HTTP server to:

- Add `_links` to all responses with navigation links
- Create a root endpoint `GET /api` that lists all available resources
- Make every response include enough information to navigate the API

### Why This Matters

- **Discoverability**: Clients can explore the API starting from `/api`
- **Self-Documenting**: No need for separate API documentation
- **Decoupling**: Clients follow links instead of hardcoding URLs
- **IoT Relevance**: Devices on local network have no docs site - the API IS the docs

### HATEOAS Concepts

**HATEOAS** (Hypermedia As The Engine Of Application State) means every response includes links to related resources:

```json
{
  "id": 0,
  "type": "light",
  "value": 2847,
  "_links": {
    "self": {"href": "/api/sensors/0"},
    "collection": {"href": "/api/sensors"}
  }
}
```

A client starts at `/api` and follows links to discover everything. No hardcoded URLs needed.

### Implementation Steps

#### Step 1: Add Root API Endpoint

Add to `http_server.c`:

```c
// ---- GET /api ----

static esp_err_t get_api_root_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "Geekhouse API");
    cJSON_AddStringToObject(root, "version", "1.0.0");
    cJSON_AddStringToObject(root, "description",
        "ESP32-C3 sensor and actuator control");

    cJSON *links = cJSON_AddObjectToObject(root, "_links");

    cJSON *self = cJSON_AddObjectToObject(links, "self");
    cJSON_AddStringToObject(self, "href", "/api");

    cJSON *sensors = cJSON_AddObjectToObject(links, "sensors");
    cJSON_AddStringToObject(sensors, "href", "/api/sensors");
    cJSON_AddStringToObject(sensors, "title", "All sensor readings");

    cJSON *leds = cJSON_AddObjectToObject(links, "leds");
    cJSON_AddStringToObject(leds, "href", "/api/leds");
    cJSON_AddStringToObject(leds, "title", "All LED states and control");

    cJSON *system = cJSON_AddObjectToObject(links, "system");
    cJSON_AddStringToObject(system, "href", "/api/system");
    cJSON_AddStringToObject(system, "title", "System information");

    return send_json_response(req, root);
}
```

#### Step 2: Add Links to Sensor Responses

Update `get_sensors_handler()` in `http_server.c`:

```c
static esp_err_t get_sensors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *sensors = cJSON_AddArrayToObject(root, "sensors");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        const sensor_info_t *info = sensor_get_info(i);
        sensor_reading_t reading;
        esp_err_t ret = sensor_read(i, &reading);

        cJSON *sensor = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensor, "id", i);
        cJSON_AddStringToObject(sensor, "type",
            info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
        cJSON_AddStringToObject(sensor, "location", info->location);

        if (ret == ESP_OK) {
            cJSON_AddNumberToObject(sensor, "raw_value", reading.raw_value);
            cJSON_AddNumberToObject(sensor, "calibrated_value",
                                    reading.calibrated_value);
            cJSON_AddStringToObject(sensor, "unit", reading.unit);
            cJSON_AddNumberToObject(sensor, "timestamp", reading.timestamp);
        }

        // Add _links to each sensor
        cJSON *links = cJSON_AddObjectToObject(sensor, "_links");
        char href[32];
        snprintf(href, sizeof(href), "/api/sensors/%d", i);
        cJSON *self_link = cJSON_AddObjectToObject(links, "self");
        cJSON_AddStringToObject(self_link, "href", href);

        cJSON_AddItemToArray(sensors, sensor);
    }

    // Add _links to collection
    cJSON *links = cJSON_AddObjectToObject(root, "_links");
    cJSON *self = cJSON_AddObjectToObject(links, "self");
    cJSON_AddStringToObject(self, "href", "/api/sensors");
    cJSON *up = cJSON_AddObjectToObject(links, "up");
    cJSON_AddStringToObject(up, "href", "/api");
    cJSON_AddStringToObject(up, "title", "API root");

    return send_json_response(req, root);
}
```

#### Step 3: Add Links to Single Sensor Response

Update `get_sensor_by_id_handler()`:

```c
static esp_err_t get_sensor_by_id_handler(httpd_req_t *req)
{
    const char *uri = req->uri;
    int id = uri[strlen("/api/sensors/")] - '0';

    if (id < 0 || id >= SENSOR_COUNT) {
        return send_error_response(req, 404, "Sensor not found");
    }

    const sensor_info_t *info = sensor_get_info(id);
    sensor_reading_t reading;
    esp_err_t ret = sensor_read(id, &reading);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddStringToObject(root, "type",
        info->type == SENSOR_TYPE_LIGHT ? "light" : "water");
    cJSON_AddStringToObject(root, "location", info->location);

    if (ret == ESP_OK) {
        cJSON_AddNumberToObject(root, "raw_value", reading.raw_value);
        cJSON_AddNumberToObject(root, "calibrated_value",
                                reading.calibrated_value);
        cJSON_AddStringToObject(root, "unit", reading.unit);
        cJSON_AddNumberToObject(root, "timestamp", reading.timestamp);
    }

    // Add _links
    cJSON *links = cJSON_AddObjectToObject(root, "_links");
    char href[32];
    snprintf(href, sizeof(href), "/api/sensors/%d", id);
    cJSON *self = cJSON_AddObjectToObject(links, "self");
    cJSON_AddStringToObject(self, "href", href);
    cJSON *collection = cJSON_AddObjectToObject(links, "collection");
    cJSON_AddStringToObject(collection, "href", "/api/sensors");
    cJSON_AddStringToObject(collection, "title", "All sensors");

    return send_json_response(req, root);
}
```

#### Step 4: Add Links to LED Responses

Update `get_leds_handler()` and `post_led_handler()` similarly:

```c
static esp_err_t get_leds_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *leds = cJSON_AddArrayToObject(root, "leds");

    for (int i = 0; i < LED_COUNT; i++) {
        const led_info_t *info = led_get_info(i);
        bool state;
        led_get_state(i, &state);

        cJSON *led = cJSON_CreateObject();
        cJSON_AddNumberToObject(led, "id", i);
        cJSON_AddStringToObject(led, "color", info->color);
        cJSON_AddStringToObject(led, "location", info->location);
        cJSON_AddBoolToObject(led, "state", state);

        // Add _links with action hints
        cJSON *links = cJSON_AddObjectToObject(led, "_links");
        char href[32];
        snprintf(href, sizeof(href), "/api/leds/%d", i);

        cJSON *self_link = cJSON_AddObjectToObject(links, "self");
        cJSON_AddStringToObject(self_link, "href", href);
        cJSON_AddStringToObject(self_link, "method", "GET");

        cJSON *control = cJSON_AddObjectToObject(links, "control");
        cJSON_AddStringToObject(control, "href", href);
        cJSON_AddStringToObject(control, "method", "POST");
        cJSON_AddStringToObject(control, "title", "Control LED");
        cJSON_AddStringToObject(control, "accepts",
            "{\"action\": \"on|off|toggle\"}");

        cJSON_AddItemToArray(leds, led);
    }

    // Collection links
    cJSON *links = cJSON_AddObjectToObject(root, "_links");
    cJSON *self = cJSON_AddObjectToObject(links, "self");
    cJSON_AddStringToObject(self, "href", "/api/leds");
    cJSON *up = cJSON_AddObjectToObject(links, "up");
    cJSON_AddStringToObject(up, "href", "/api");
    cJSON_AddStringToObject(up, "title", "API root");

    return send_json_response(req, root);
}
```

For `post_led_handler()`, add links to the response at the end (before `send_json_response`):

```c
    // Add _links to response
    cJSON *links = cJSON_AddObjectToObject(root, "_links");
    char href[32];
    snprintf(href, sizeof(href), "/api/leds/%d", id);
    cJSON *self = cJSON_AddObjectToObject(links, "self");
    cJSON_AddStringToObject(self, "href", href);
    cJSON *collection = cJSON_AddObjectToObject(links, "collection");
    cJSON_AddStringToObject(collection, "href", "/api/leds");
```

#### Step 5: Register the Root Endpoint

In `http_server_start()`, add to the `uris` array:

```c
    {
        .uri = "/api",
        .method = HTTP_GET,
        .handler = get_api_root_handler,
    },
```

#### Step 6: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Testing

Start from the root and follow links:

```bash
# Discover the API
curl http://192.168.1.42/api | jq .

# Follow the "sensors" link
curl http://192.168.1.42/api/sensors | jq .

# Follow a sensor's "self" link
curl http://192.168.1.42/api/sensors/0 | jq .

# Follow "up" link back to collection
curl http://192.168.1.42/api/sensors | jq '._links.up'
```

### Expected Response: GET /api

```json
{
  "name": "Geekhouse API",
  "version": "1.0.0",
  "description": "ESP32-C3 sensor and actuator control",
  "_links": {
    "self": {"href": "/api"},
    "sensors": {
      "href": "/api/sensors",
      "title": "All sensor readings"
    },
    "leds": {
      "href": "/api/leds",
      "title": "All LED states and control"
    },
    "system": {
      "href": "/api/system",
      "title": "System information"
    }
  }
}
```

### Expected Response: GET /api/leds

```json
{
  "leds": [
    {
      "id": 0,
      "color": "yellow",
      "location": "roof",
      "state": false,
      "_links": {
        "self": {"href": "/api/leds/0", "method": "GET"},
        "control": {
          "href": "/api/leds/0",
          "method": "POST",
          "title": "Control LED",
          "accepts": "{\"action\": \"on|off|toggle\"}"
        }
      }
    }
  ],
  "_links": {
    "self": {"href": "/api/leds"},
    "up": {"href": "/api", "title": "API root"}
  }
}
```

### Learning Points

1. **HATEOAS Link Relations**:
   - `self` - The canonical URL of this resource
   - `collection` - The collection this item belongs to
   - `up` - The parent resource
   - `control` - An action that can be performed (custom relation)

2. **Why HATEOAS for IoT**:
   - Devices often have no documentation website
   - `curl http://device-ip/api` tells you everything
   - Tools like HAL browsers can auto-generate UIs
   - Version changes don't break clients that follow links

3. **Implementation Notes**:
   - Links add ~100-200 bytes per response (acceptable for local network)
   - The `method` and `accepts` hints help clients know what to POST
   - Keep link generation consistent (helper functions help)
   - On memory-constrained devices, links can be optional (controlled by query param)

### Extension Ideas

- Add `_embedded` for including related resources inline
- Add pagination links for large collections (`next`, `prev`)
- Create a helper function to generate standard link blocks
- Add `_links` to the system endpoint too
- Serve a HAL browser at `/` for interactive API exploration

---

## Task 5: MQTT Client (Medium-Hard - 45 mins)

### Learning Goal

Implement an MQTT client that publishes sensor data and subscribes to LED control commands. Learn the publish/subscribe messaging pattern and MQTT-specific concepts like topics, QoS, and last-will messages.

### What to Build

An MQTT client that:

- Publishes sensor readings to `geekhouse/sensors/{type}` every 5 seconds
- Subscribes to `geekhouse/leds/+/set` for LED control
- Sends a last-will message for online/offline status
- Uses event groups to coordinate with WiFi status

### Why This Matters

- **Push Model**: MQTT pushes data to subscribers (vs. HTTP where clients must poll)
- **Efficient**: Tiny overhead, perfect for constrained devices
- **IoT Standard**: Used by Home Assistant, AWS IoT, Azure IoT, etc.
- **Bidirectional**: Both publish sensor data AND receive commands

### MQTT Concepts

**MQTT** is a lightweight publish/subscribe messaging protocol:

- **Broker**: Central server that routes messages (e.g., Mosquitto)
- **Topic**: Hierarchical address (e.g., `geekhouse/sensors/light`)
- **Publish**: Send a message to a topic
- **Subscribe**: Receive messages from a topic pattern
- **QoS**: Delivery guarantee (0=at most once, 1=at least once, 2=exactly once)
- **Last Will**: Message sent by broker if client disconnects unexpectedly

**Topic Wildcards**:

- `+` matches one level: `geekhouse/leds/+/set` matches `geekhouse/leds/0/set`
- `#` matches all remaining levels: `geekhouse/#` matches everything

### Prerequisites

You need an MQTT broker. Install Mosquitto on your computer:

```bash
# Fedora/RHEL
sudo dnf install mosquitto mosquitto-clients

# Ubuntu/Debian
sudo apt install mosquitto mosquitto-clients

# macOS
brew install mosquitto

# Start the broker
sudo systemctl start mosquitto
# or
mosquitto -v
```

### Implementation Steps

#### Step 1: Update idf_component.yml

Add MQTT dependency to `main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: ">=5.0.0"
  espressif/cjson:
    version: "^1.7.18"
  espressif/mqtt:
    version: "^2.0.0"
```

Note: In ESP-IDF v5.x, you can also use the built-in `esp_mqtt` component instead. The `espressif/mqtt` component from the registry provides the same API with potentially newer features.

#### Step 2: Add MQTT Broker Config to Kconfig.projbuild

Update `main/Kconfig.projbuild`:

```
menu "Geekhouse Configuration"

    config GEEKHOUSE_WIFI_SSID
        string "WiFi SSID"
        default "myssid"
        help
            Default WiFi network name.

    config GEEKHOUSE_WIFI_PASSWORD
        string "WiFi Password"
        default "mypassword"
        help
            Default WiFi password.

    config GEEKHOUSE_MQTT_BROKER_URL
        string "MQTT Broker URL"
        default "mqtt://192.168.1.100"
        help
            MQTT broker URL. Use mqtt:// for unencrypted,
            mqtts:// for TLS. Example: mqtt://192.168.1.100

endmenu
```

#### Step 3: Create mqtt_manager.h

```c
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"

/**
 * Initialize and start the MQTT client
 *
 * Connects to the broker specified in Kconfig.
 * Publishes online status and subscribes to LED control topics.
 *
 * Must be called after WiFi is connected.
 *
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_init(void);

/**
 * Publish a sensor reading
 *
 * Publishes to geekhouse/sensors/{type} in JSON format.
 *
 * @param sensor_type "light" or "water"
 * @param raw_value Raw ADC value
 * @param calibrated_value Calibrated value
 * @param unit Unit string
 * @return ESP_OK on success
 */
esp_err_t mqtt_manager_publish_sensor(const char *sensor_type,
                                       int raw_value,
                                       float calibrated_value,
                                       const char *unit);

/**
 * Check if MQTT is connected
 *
 * @return true if connected to broker
 */
bool mqtt_manager_is_connected(void);

#endif // MQTT_MANAGER_H
```

#### Step 4: Create mqtt_manager.c

```c
#include "mqtt_manager.h"
#include "actuators.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MQTT_MGR";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

// Topic prefix
#define TOPIC_PREFIX "geekhouse"

// Subscribe topics
#define TOPIC_LED_SET TOPIC_PREFIX "/leds/+/set"

// Publish topics
#define TOPIC_STATUS TOPIC_PREFIX "/status"

/**
 * Handle incoming MQTT message for LED control
 *
 * Expected topic: geekhouse/leds/{id}/set
 * Expected payload: {"action": "on"} or {"action": "off"} or {"action": "toggle"}
 */
static void handle_led_command(const char *topic, int topic_len,
                                const char *data, int data_len)
{
    // Extract LED ID from topic
    // Topic format: geekhouse/leds/{id}/set
    // Find the ID between the last two slashes
    char topic_str[64] = {0};
    int copy_len = topic_len < (int)sizeof(topic_str) - 1 ? topic_len : (int)sizeof(topic_str) - 1;
    strncpy(topic_str, topic, copy_len);

    // Find LED ID: skip "geekhouse/leds/" (15 chars), take next char
    if (topic_len < 17) {
        ESP_LOGW(TAG, "Topic too short: %.*s", topic_len, topic);
        return;
    }
    int led_id = topic_str[15] - '0';

    if (led_id < 0 || led_id >= LED_COUNT) {
        ESP_LOGW(TAG, "Invalid LED ID %d in topic", led_id);
        return;
    }

    // Parse JSON payload
    char payload[128] = {0};
    int payload_len = data_len < (int)sizeof(payload) - 1 ? data_len : (int)sizeof(payload) - 1;
    strncpy(payload, data, payload_len);

    cJSON *json = cJSON_Parse(payload);
    if (json == NULL) {
        ESP_LOGW(TAG, "Invalid JSON payload: %s", payload);
        return;
    }

    cJSON *action = cJSON_GetObjectItem(json, "action");
    if (!cJSON_IsString(action)) {
        ESP_LOGW(TAG, "Missing 'action' in payload");
        cJSON_Delete(json);
        return;
    }

    // Execute action
    esp_err_t ret = ESP_FAIL;
    if (strcmp(action->valuestring, "on") == 0) {
        ret = led_on(led_id);
    } else if (strcmp(action->valuestring, "off") == 0) {
        ret = led_off(led_id);
    } else if (strcmp(action->valuestring, "toggle") == 0) {
        ret = led_toggle(led_id);
    } else {
        ESP_LOGW(TAG, "Unknown action: %s", action->valuestring);
    }

    cJSON_Delete(json);

    if (ret == ESP_OK) {
        bool state;
        led_get_state(led_id, &state);
        ESP_LOGI(TAG, "LED %d set to %s via MQTT", led_id,
                 state ? "ON" : "OFF");

        // Publish updated state back
        char state_topic[64];
        snprintf(state_topic, sizeof(state_topic),
                 TOPIC_PREFIX "/leds/%d/state", led_id);
        const char *state_str = state ? "{\"state\":true}" : "{\"state\":false}";
        esp_mqtt_client_publish(s_client, state_topic, state_str, 0, 1, true);
    }
}

/**
 * MQTT event handler
 *
 * Called by the MQTT client library for connection, disconnection,
 * message received, etc.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            s_connected = true;

            // Publish online status (retained)
            esp_mqtt_client_publish(s_client, TOPIC_STATUS,
                                   "{\"online\":true}", 0, 1, true);

            // Subscribe to LED control
            esp_mqtt_client_subscribe(s_client, TOPIC_LED_SET, 1);
            ESP_LOGI(TAG, "Subscribed to %s", TOPIC_LED_SET);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            s_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscription confirmed (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Message received on topic: %.*s",
                     event->topic_len, event->topic);

            // Check if it's a LED command
            if (strncmp(event->topic, TOPIC_PREFIX "/leds/",
                       strlen(TOPIC_PREFIX "/leds/")) == 0) {
                handle_led_command(event->topic, event->topic_len,
                                  event->data, event->data_len);
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

esp_err_t mqtt_manager_init(void)
{
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_GEEKHOUSE_MQTT_BROKER_URL,
        .credentials.client_id = "geekhouse-esp32c3",
        // Last Will and Testament: broker publishes this if we disconnect
        .session.last_will = {
            .topic = TOPIC_STATUS,
            .msg = "{\"online\":false}",
            .msg_len = 0,      // 0 = use strlen
            .qos = 1,
            .retain = true,    // Retained so new subscribers see status
        },
    };

    ESP_LOGI(TAG, "Connecting to MQTT broker: %s",
             CONFIG_GEEKHOUSE_MQTT_BROKER_URL);

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }

    // Register event handler
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    // Start the client (connects in background)
    esp_err_t ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client");
        return ret;
    }

    return ESP_OK;
}

esp_err_t mqtt_manager_publish_sensor(const char *sensor_type,
                                       int raw_value,
                                       float calibrated_value,
                                       const char *unit)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    // Build topic
    char topic[64];
    snprintf(topic, sizeof(topic), TOPIC_PREFIX "/sensors/%s", sensor_type);

    // Build JSON payload
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "raw_value", raw_value);
    cJSON_AddNumberToObject(json, "calibrated_value", calibrated_value);
    cJSON_AddStringToObject(json, "unit", unit);
    cJSON_AddNumberToObject(json, "timestamp",
                            (double)(esp_timer_get_time() / 1000));

    char *payload = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Publish with QoS 0 (fire and forget - fine for periodic sensor data)
    int msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, 0, false);
    free(payload);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish to %s", topic);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool mqtt_manager_is_connected(void)
{
    return s_connected;
}
```

#### Step 5: Create MQTT Publishing Task

You can either add publishing to the existing sensor_task or create a new task. Here's a simple approach using a dedicated publishing task.

Add to `main.c`:

```c
#include "mqtt_manager.h"

/**
 * MQTT publish task
 *
 * Periodically reads sensor data and publishes via MQTT.
 * Runs independently from the sensor task.
 */
static void mqtt_publish_task(void *pvParameters)
{
    (void)pvParameters;

    // Wait a bit for MQTT to connect
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        if (mqtt_manager_is_connected()) {
            // Read and publish each sensor
            sensor_reading_t reading;

            if (sensor_read(SENSOR_LIGHT_ROOF, &reading) == ESP_OK) {
                mqtt_manager_publish_sensor("light",
                    reading.raw_value, reading.calibrated_value,
                    reading.unit);
            }

            if (sensor_read(SENSOR_WATER_ROOF, &reading) == ESP_OK) {
                mqtt_manager_publish_sensor("water",
                    reading.raw_value, reading.calibrated_value,
                    reading.unit);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));  // Publish every 5 seconds
    }
}

void app_main(void)
{
    // ... existing init ...

    if (bits & WIFI_CONNECTED_BIT) {
        // ... HTTP server start ...

        // Start MQTT
        ESP_LOGI(TAG, "Starting MQTT client...");
        ESP_ERROR_CHECK(mqtt_manager_init());

        // Create MQTT publish task
        xTaskCreate(mqtt_publish_task, "mqtt_pub", 4096, NULL, 3, NULL);
    }

    // ... rest of app_main ...
}
```

#### Step 6: Update CMakeLists.txt

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
        "wifi_config.c"
        "wifi_manager.c"
        "http_server.c"
        "mqtt_manager.c"       # Add this
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_http_server
)
```

Note: The `espressif/mqtt` component from `idf_component.yml` is automatically linked. You don't need to add it to `PRIV_REQUIRES`.

#### Step 7: Set MQTT Broker URL

```bash
idf.py menuconfig
```

Navigate to `Geekhouse Configuration` → `MQTT Broker URL` and set it to your computer's IP (e.g., `mqtt://192.168.1.100`).

#### Step 8: Build and Test

```bash
idf.py build
idf.py flash monitor
```

### Testing with Mosquitto

Open separate terminals for subscribing and publishing:

**Terminal 1 - Subscribe to all Geekhouse topics:**

```bash
mosquitto_sub -h localhost -t "geekhouse/#" -v
```

Expected output (sensor data every 5 seconds):

```none
geekhouse/status {"online":true}
geekhouse/sensors/light {"raw_value":2847,"calibrated_value":2847.0,"unit":"raw","timestamp":12345}
geekhouse/sensors/water {"raw_value":1534,"calibrated_value":1534.0,"unit":"raw","timestamp":12347}
geekhouse/sensors/light {"raw_value":2851,"calibrated_value":2851.0,"unit":"raw","timestamp":17345}
geekhouse/sensors/water {"raw_value":1530,"calibrated_value":1530.0,"unit":"raw","timestamp":17347}
```

**Terminal 2 - Control LEDs:**

```bash
# Turn LED 0 on
mosquitto_pub -h localhost -t "geekhouse/leds/0/set" \
    -m '{"action": "on"}'

# Toggle LED 1
mosquitto_pub -h localhost -t "geekhouse/leds/1/set" \
    -m '{"action": "toggle"}'

# Turn LED 0 off
mosquitto_pub -h localhost -t "geekhouse/leds/0/set" \
    -m '{"action": "off"}'
```

**Terminal 1 should also show the state responses:**

```none
geekhouse/leds/0/state {"state":true}
geekhouse/leds/1/state {"state":true}
geekhouse/leds/0/state {"state":false}
```

**Test Last Will - Unplug ESP32 or reset it:**

```none
geekhouse/status {"online":false}
```

(The broker publishes this automatically when the client disconnects unexpectedly.)

### Learning Points

1. **MQTT vs HTTP**:
   - HTTP: Request/response, client pulls data
   - MQTT: Publish/subscribe, data pushed to subscribers
   - HTTP better for on-demand queries and REST APIs
   - MQTT better for continuous data streams and event-driven control
   - Use both! HTTP for configuration, MQTT for real-time data

2. **QoS Levels**:
   - QoS 0: Fire and forget (best for periodic sensor data)
   - QoS 1: At least once delivery (good for commands)
   - QoS 2: Exactly once (rarely needed, highest overhead)
   - Higher QoS = more memory and bandwidth

3. **Retained Messages**:
   - Broker stores the last retained message per topic
   - New subscribers immediately get the last value
   - Perfect for status (online/offline) and LED state
   - Set `retain = true` in publish call

4. **Last Will and Testament (LWT)**:
   - Configured at connection time
   - Broker publishes it if client disconnects ungracefully
   - Essential for device online/offline tracking
   - Must be retained for subscribers to see it later

5. **Topic Design**:
   - Use hierarchical topics: `{project}/{resource_type}/{resource_id}/{action}`
   - Subscribe topics: `geekhouse/leds/+/set` (+ matches any LED ID)
   - State topics: `geekhouse/leds/0/state` (specific LED state)
   - Sensor topics: `geekhouse/sensors/light` (by type, not ID)

### Extension Ideas

- Add MQTT reconnection logic (coordinate with WiFi event group)
- Publish sensor statistics (min/max/avg) periodically
- Add configurable publish interval via MQTT command
- Implement MQTT over TLS (mqtts://)
- Add device discovery via MQTT (Home Assistant auto-discovery)
- Create a simple Python script to subscribe and plot sensor data

---

## Summary and Next Steps

Congratulations on completing Phase 3! You now have a fully network-connected IoT device with:

- NVS configuration storage
- WiFi with auto-reconnect
- REST API with JSON and HATEOAS
- MQTT publish/subscribe
- Remote LED control via HTTP and MQTT
- Sensor data accessible from anywhere on your network

### Architecture Overview

```
                    +-----------+
                    |  Router   |
                    +-----+-----+
                          |
              +-----------+-----------+
              |                       |
     +--------+--------+    +--------+--------+
     | ESP32-C3        |    | Your Computer   |
     |                 |    |                 |
     | sensor_task     |    | curl / browser  |
     | display_task    |    | mosquitto_sub   |
     | stats_task      |    | mosquitto_pub   |
     | reporter_task   |    |                 |
     | HTTP server     |<-->| GET/POST /api/* |
     | MQTT client  ---|<-->| MQTT broker     |
     | wifi_manager    |    |                 |
     | wifi_config/NVS |    |                 |
     +-----------------+    +-----------------+
```

### Recommended Learning Path

1. **Start with Task 1** (NVS) - Foundation for all network tasks
2. **Do Task 2** (WiFi) - Required for everything else
3. **Do Task 3** (HTTP) - Most immediately useful and testable
4. **Do Task 4** (HATEOAS) - Quick enhancement, good REST practices
5. **Do Task 5** (MQTT) - Enables real-time monitoring and control

### Ready for Phase 4?

With network connectivity complete, Phase 4 could cover:

- OTA (Over-The-Air) firmware updates
- mDNS for `geekhouse.local` discovery
- Home Assistant integration
- Persistent data logging (SD card or cloud)
- Power management and deep sleep
- Security hardening (TLS, authentication)

### Additional Resources

- **ESP-IDF WiFi Guide**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/wifi.html
- **ESP-IDF HTTP Server**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/protocols/esp_http_server.html
- **ESP-IDF MQTT**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/protocols/mqtt.html
- **NVS Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/storage/nvs_flash.html
- **cJSON GitHub**: https://github.com/DaveGamble/cJSON
- **MQTT Specification**: https://mqtt.org/mqtt-specification/
