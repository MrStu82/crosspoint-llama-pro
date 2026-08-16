#pragma once
// Test stub. GameActivity.cpp/Activity.cpp only ever take a MappedInputManager&
// (never construct one), so this stub's constructor shape is free to differ
// from the real HalGPIO-coupled one -- it's default-constructible so the
// harness can build one directly with no HalGPIO stub needed at all.
//
// Full real Button enum copied verbatim (confirmed via read of the real
// src/MappedInputManager.h) since GameActivity.cpp's CorruptSaveNotice/loop()
// branch switches on Button::Up/Down/Confirm/Back by value -- an incomplete
// or reordered enum would silently break those comparisons.
//
// wasReleased() is the only method this harness actually drives: settable via
// the public `nextReleased` field, consumed (reset to None) on first read so
// a single scripted "release" only fires once per simulated frame, matching
// how a real button-release event would only be true for one loop() call.
// Every other method is an inert no-op/false-return copied from the
// test/sponsor_hp_clamp/stubs/MappedInputManager.h precedent -- GameActivity.cpp
// itself only calls wasReleased()/wasScreenTapped() (confirmed via grep), the
// rest exist purely because GameRenderer.h's hitTestControls(..., Button&)
// signature references the type.
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

  MappedInputManager() = default;

  // Test-only knob: set to the button a scripted frame should report as
  // released, then call the real loop(). Auto-clears back to nullopt-ish
  // (kNone) after being read once via wasReleased(), so it behaves like a
  // one-frame edge event, not a held-down level.
  static constexpr Button kNone = static_cast<Button>(0xFF);
  Button nextReleased = kNone;

  void update() const {}
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button b) {
    if (nextReleased != kNone && b == nextReleased) {
      nextReleased = kNone;
      return true;
    }
    return false;
  }
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
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const {
    return Labels{back, confirm, previous, next};
  }
  Labels mapDirectionalLabels(const char* back, const char* confirm, const char*, const char*, const char*,
                               const char*) const {
    return Labels{back, confirm, nullptr, nullptr};
  }
  int getPressedFrontButton() const { return -1; }
  [[nodiscard]] bool isNavDirectionSwapped() const { return false; }
};
