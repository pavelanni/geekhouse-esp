#include "sensor_data_shared.h"

// Global shared sensor data
shared_sensor_data_t g_shared_sensor_data = {0};
SemaphoreHandle_t g_shared_data_mutex = NULL;
