#include "time_sync.h"

#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "TIME_SYNC";
static bool s_time_synced = false;

/**
 * Callback when time is synchronized
 *
 * Called by the SNTP subsystem after each successful sync.
 */
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized via NTP");
    s_time_synced = true;

    // Print current time
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);
}

esp_err_t time_sync_init(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");

    // Set timezone (change to your timezone)
    // Format: POSIX TZ string
    // Examples:
    //   "EST5EDT,M3.2.0,M11.1.0"  — US Eastern
    //   "CET-1CEST,M3.5.0,M10.5.0/3" — Central European
    //   "PST8PDT,M3.2.0,M11.1.0" — US Pacific
    //   "UTC0" — UTC (no DST)
    setenv("TZ", "UTC0", 1);
    tzset();

    // Configure SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP initialized, waiting for sync...");
    return ESP_OK;
}

bool time_sync_is_synced(void) {
    return s_time_synced;
}
