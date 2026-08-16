#pragma once
// Shadow of src/MappedInputManager.h for the sponsor HP-clamp harness. GameActivity.cpp is
// compiled unmodified in full (not just its render() path), so every method signature it or
// GameRenderer.h reference must exist to satisfy the compiler even though the harness's own
// call path (onEnter() -> loadOrGenerateLevel()) never touches mappedInput at all. Grep of
// GameActivity.cpp shows only wasReleased()/wasScreenTapped() are actually called there; the
// rest of the real interface below (Button/SwipeDir enums, Labels, RowTouch/colTouch etc.) is
// present purely so declarations elsewhere (e.g. GameRenderer.h's hitTestControls(Button&))
// resolve. All bodies are inert no-ops/false-returns -- same technique as
// test/game_title_render/stubs/MappedInputManager.h.
#include "HalGPIO.h"

#include <climits>
#include <cstdint>

class GfxRenderer;

class MappedInputManager {
 public:
  enum class Button {
    Back,
    Confirm,
    Left,
    Right,
    Up,
    Down,
    Power,
    PageBack,
    PageForward,
    NavNext,
    NavPrevious,
    ScreenLeft,
    ScreenRight,
    ScreenUp,
    ScreenDown
  };
  enum class SwipeDir { None, Left, Right, Up, Down };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  enum class RowTouch : uint8_t { None, Down, Tap };

  MappedInputManager(HalGPIO& gpio, const GfxRenderer& renderer) : gpio(gpio), renderer(renderer) {}

  void update() const { gpio.update(); }
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button) const { return false; }
  bool isPressed(Button) const { return false; }
  bool hasTouch() const { return false; }
  bool wasScreenTapped(int& x, int& y) const {
    (void)x;
    (void)y;
    return false;
  }
  bool wasScreenTouchDown(int& x, int& y) const {
    (void)x;
    (void)y;
    return false;
  }
  bool isScreenTouchHeld(int& x, int& y) const {
    (void)x;
    (void)y;
    return false;
  }
  bool wasTapInRect(int, int, int, int) const { return false; }
  bool wasListItemTapped(int&, int, int, int, int, bool) const { return false; }
  bool wasListItemTouchedDown(int&, int, int, int, int, bool) const { return false; }

  RowTouch rowTouch(int&, int, int, int, int xStart = 0, int xEnd = INT32_MAX, int rowHeight = 0) const {
    (void)xStart;
    (void)xEnd;
    (void)rowHeight;
    return RowTouch::None;
  }
  RowTouch colTouch(int&, int, int, int, int, int, int colWidth = 0) const {
    (void)colWidth;
    return RowTouch::None;
  }

  SwipeDir wasSwipe() const { return SwipeDir::None; }
  bool wasHomeGesture() const { return false; }
  bool wasMenuGesture() const { return false; }
  bool wasBrightnessGesture() const { return false; }
  bool wasBrightnessSheetGesture() const { return false; }
  bool wasAnyPressed() const { return false; }
  bool wasAnyReleased() const { return false; }
  unsigned long getHeldTime() const { return 0; }
  const GfxRenderer& getRenderer() const { return renderer; }
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const {
    return Labels{back, confirm, previous, next};
  }
  Labels mapDirectionalLabels(const char* back, const char* confirm, const char*, const char*, const char*,
                               const char*) const {
    return Labels{back, confirm, nullptr, nullptr};
  }
  int getPressedFrontButton() const { return -1; }
  [[nodiscard]] bool isNavDirectionSwapped() const { return false; }

 private:
  HalGPIO& gpio;
  const GfxRenderer& renderer;
};
