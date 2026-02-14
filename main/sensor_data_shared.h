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

#endif  // SENSOR_DATA_SHARED_H
