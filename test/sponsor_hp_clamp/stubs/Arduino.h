#pragma once
// Minimal host stand-in, same precedent as build-scripts/halclock_test/stubs/Arduino.h.
// Force-included (-include) on every TU so real, unmodified .cpp files that assume the
// Arduino/ESP32 global environment (millis(), ESP.restart(), <cassert>/<cmath> transitively
// pulled in on-device but not on host) compile unchanged.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

inline unsigned long g_fakeMillis = 0;
inline unsigned long millis() { return g_fakeMillis; }
inline unsigned long micros() { return g_fakeMillis * 1000UL; }

struct EspClass {
  void restart() { std::abort(); }
  uint32_t getFreeHeap() { return 8u * 1024u * 1024u; }  // plenty, host has no real constraint
};
inline EspClass ESP;
