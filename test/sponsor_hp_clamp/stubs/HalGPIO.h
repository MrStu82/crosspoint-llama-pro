#pragma once
// Trivial shadow of lib/hal/HalGPIO.h. GfxRenderer.cpp includes <HalGPIO.h> but never
// references a symbol from it (confirmed dead include). Our own stub MappedInputManager.h
// (below) also needs a HalGPIO& member to construct -- given a real HalGPIO() constructor
// but no method bodies (nothing in the harness's runtime path calls into it).
#include <cstdint>

class HalGPIO {
 public:
  HalGPIO() = default;
  void update() {}
};

inline HalGPIO gpio;
