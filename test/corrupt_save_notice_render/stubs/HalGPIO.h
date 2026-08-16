#pragma once
// Trivial shadow of lib/hal/HalGPIO.h, extended from test/game_title_render/stubs/HalGPIO.h
// with hasTouch() -- the real UITheme.cpp's UITheme::getMetrics() calls gpio.hasTouch() in a
// TU that's linked in whole (even though getMetrics() itself is never called by
// drawCorruptSaveNotice()'s path, the symbol still needs to resolve).
#include <cstdint>

class HalGPIO {
 public:
  HalGPIO() = default;
  void update() {}
  bool hasTouch() const { return false; }
};

inline HalGPIO gpio;
