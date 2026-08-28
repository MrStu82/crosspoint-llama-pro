#pragma once

#include <GfxRenderer.h>

class MappedInputManager {};

class Activity {
 public:
  Activity(const char*, GfxRenderer& renderer, MappedInputManager& input) : renderer(renderer), mappedInput(input) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}

 protected:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
};
