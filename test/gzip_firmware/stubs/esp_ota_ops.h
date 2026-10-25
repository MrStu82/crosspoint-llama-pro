#pragma once
#include "esp_partition.h"

// Set by the test.
extern const esp_partition_t* g_nextUpdate;
extern const esp_partition_t* g_running;
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) { return g_nextUpdate; }
inline const esp_partition_t* esp_ota_get_running_partition() { return g_running; }
