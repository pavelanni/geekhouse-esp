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

#endif  // HTTP_SERVER_H
