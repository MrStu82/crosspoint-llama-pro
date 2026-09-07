#pragma once
#if FREEINK_DEVICE_X4PRO && !defined(SIMULATOR)
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_arduino_version.h>
#include <PowerManager.h>
#include <BoardConfig.h>

namespace x4pro_sleep {
// Both X4 Pro frontlight outputs are active-high. Keep settings untouched.
inline void holdFrontlightOff() {
  for (int pin : {8, 9}) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(pin);
#else
    ledcDetachPin(pin);
#endif
    gpio_hold_dis(static_cast<gpio_num_t>(pin));
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    gpio_hold_en(static_cast<gpio_num_t>(pin));
  }
}
inline void releaseFrontlightHold() {
  for (int pin : {8, 9}) {
    // Establish an off level before releasing a previous sleep hold.
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    gpio_hold_dis(static_cast<gpio_num_t>(pin));
  }
}
// A stuck button must not leave an awake CPU behind a sleep-looking frame.
// If still held after 2s, sleep until RELEASE, not the already-active press.
// Release wake is rejected by the existing physical-held wake validator, which
// returns to normal press-wake sleep before storage/frontlight initialization.
inline bool armBoundedPowerWake() {
  constexpr int pin = 3;
  pinMode(pin, INPUT_PULLUP);
  const uint32_t start = millis();
  while (digitalRead(pin) == LOW && static_cast<uint32_t>(millis() - start) < 2000) delay(10);
  const bool held = digitalRead(pin) == LOW;
  freeink::PowerManager::armWakeOnPins(1ULL << pin, !held);
  return held;
}
}  // namespace x4pro_sleep
#endif
