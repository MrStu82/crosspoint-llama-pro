#pragma once
// Minimal host stand-in for the ESP-IDF driver/gpio.h BoardConfig.h includes
// (for gpio_hold_dis() inside BoardConfig::releaseSdRail(), an inline
// function whose body must still typecheck on host even though this harness
// never calls it). gpio_num_t only needs to exist and be castable from int8_t.
#include <cstdint>

typedef int gpio_num_t;

inline void gpio_hold_dis(gpio_num_t) {}
