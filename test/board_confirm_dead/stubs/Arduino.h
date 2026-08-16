#pragma once
// Minimal host stand-in, same precedent as the other test/*/stubs/Arduino.h
// files. Unlike those (which only need millis()/micros() for GameState-side
// seeding), BoardConfig.h's inline functions -- holdPowerRails(),
// releaseSdRail(), serialTransport() -- reference pinMode/digitalWrite/
// OUTPUT/HIGH/LOW/Serial directly in their bodies. Those functions are never
// called by this harness (it only reads XTEINK_X4_PRO's constexpr data), but
// an inline function's body still has to typecheck wherever it's defined, so
// these need to exist as real declarations, not just be reachable at link
// time. Bodies are inert -- correctness here doesn't matter, only that they
// compile with the right signatures.
#include <cstdint>
#include <initializer_list>

inline unsigned long g_fakeMillis = 0;
inline unsigned long millis() { return g_fakeMillis; }
inline unsigned long micros() { return g_fakeMillis * 1000UL; }

constexpr int OUTPUT = 1;
constexpr int INPUT = 0;
constexpr int HIGH = 1;
constexpr int LOW = 0;

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return LOW; }

struct FakeSerial {
  template <typename T>
  FakeSerial& operator<<(const T&) {
    return *this;
  }
};
inline FakeSerial Serial;
