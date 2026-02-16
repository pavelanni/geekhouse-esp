#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>

#include "esp_err.h"

/**
 * Initialize SNTP and start time synchronization
 *
 * Configures the SNTP client to sync from pool.ntp.org.
 * Time sync happens asynchronously after this call returns.
 *
 * Must be called after WiFi is connected.
 *
 * @return ESP_OK on success
 */
esp_err_t time_sync_init(void);

/**
 * Check if time has been synchronized
 *
 * @return true if system clock has been set via NTP
 */
bool time_sync_is_synced(void);

#endif  // TIME_SYNC_H
