#pragma once
// Minimal host stand-in for esp_rom_sys.h (esp_rom_printf, used inside
// BoardConfig::holdPowerRails(), same "must typecheck though never called"
// reasoning as driver/gpio.h above).
#include <cstdio>

template <typename... Args>
inline void esp_rom_printf(const char* fmt, Args... args) {
  std::fprintf(stderr, fmt, args...);
}
