#pragma once

#include <cstdint>

// App-layer frontlight singleton. Wraps the freeink-sdk FrontlightManager (which is
// already a complete, self-guarded abstraction — present()/hasColorTemperature() make it
// safe to construct and call on any board, frontlight or not). No further Hal wrapper is
// needed on top of it; this header just gives the rest of the app an extern to reach it,
// mirroring how activityManager/mappedInputManager are declared (defined in main.cpp,
// extern'd from an app-layer header) rather than editing the vendored SDK.
//
// Every write to the frontlight MUST go through this instance's setBrightness()/
// setColorTemperature() — never touch LEDC/GPIO directly. If the GPIO8/GPIO9 mapping
// turns out to be wrong, the fix is one BoardConfig profile entry, not new call sites.

#ifdef SIMULATOR
// The simulator owns the visible frontlight state; this API-compatible shim
// keeps the firmware settings path intact without pulling ESP32 PWM drivers
// into the native process.
class FrontlightManager {
 public:
  bool begin() { return true; }
  void setBrightness(uint8_t value) { brightness_ = value; }
  void setColorTemperature(uint8_t value) { warm_ = value; }
  void off() { last_ = brightness_; brightness_ = 0; }
  void on() { brightness_ = last_ == 0 ? 50 : last_; }
  bool present() const { return true; }
  bool hasColorTemperature() const { return true; }
  uint8_t brightness() const { return brightness_; }
  uint8_t colorTemperature() const { return warm_; }
  bool coolChannelAttachOk() const { return true; }
  bool warmChannelAttachOk() const { return true; }

 private:
  uint8_t brightness_ = 0;
  uint8_t last_ = 50;
  uint8_t warm_ = 50;
};
#else
#include <FrontlightManager.h>
#endif

extern FrontlightManager frontlightManager;
