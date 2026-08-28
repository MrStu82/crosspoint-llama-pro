#pragma once

#include <cstdint>

struct EspStub {
  size_t getFreeHeap() const { return 1024 * 1024; }
};
inline EspStub ESP;
inline uint32_t millis() {
  static uint32_t now = 0;
  return ++now;
}
inline void vTaskDelay(int) {}
