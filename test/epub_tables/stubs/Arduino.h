#pragma once

#include <cstdint>

struct EspTableStub {
  uint32_t getFreeHeap() const { return UINT32_MAX; }
  uint32_t getMaxAllocHeap() const { return UINT32_MAX; }
};

inline EspTableStub ESP;
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
