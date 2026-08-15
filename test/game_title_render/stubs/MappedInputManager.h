#pragma once
// Shadow of src/MappedInputManager.h. GameTitleActivity::render() (the harness's actual
// runtime path) never touches mappedInput at all -- only onEnter()/loop() do, and neither
// is called by the harness. But Activity.cpp and GameTitleActivity.cpp are compiled
// unmodified, so every method signature they reference (even in code paths the harness
// never executes) must still exist to satisfy the compiler. All bodies here are inert
// no-ops/false-returns: nothing in the render path can reach them.
#include "HalGPIO.h"

#include <cstdint>

class GfxRenderer;

class MappedInputManager {
 public:
  MappedInputManager(HalGPIO& gpio, const GfxRenderer& renderer) : gpio(gpio), renderer(renderer) {}

  bool wasAnyReleased() const { return false; }
  bool wasScreenTapped(int& x, int& y) const {
    (void)x;
    (void)y;
    return false;
  }
  bool wasListItemTouchedDown(int& index, int itemCount, int selectedIndex, int listTop, int listHeight,
                               bool hasSubtitle) const {
    (void)index;
    (void)itemCount;
    (void)selectedIndex;
    (void)listTop;
    (void)listHeight;
    (void)hasSubtitle;
    return false;
  }
  bool wasListItemTapped(int& index, int itemCount, int selectedIndex, int listTop, int listHeight,
                          bool hasSubtitle) const {
    (void)index;
    (void)itemCount;
    (void)selectedIndex;
    (void)listTop;
    (void)listHeight;
    (void)hasSubtitle;
    return false;
  }

 private:
  HalGPIO& gpio;
  const GfxRenderer& renderer;
};
