#pragma once

#include <cstddef>
#include <cstdint>

struct EspStub {
  size_t getFreeHeap() const { return 1024 * 1024; }
};
inline EspStub ESP;
inline void delay(unsigned long) {}
inline long random(long upper) { return upper > 0 ? 0 : 0; }
