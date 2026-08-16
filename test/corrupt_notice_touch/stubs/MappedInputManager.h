#pragma once
// Test stub for the touch-path redness proof (corrupt_notice_touch). Same base
// as test/whole_run_corrupt_save/stubs/MappedInputManager.h (default-constructible,
// full real Button enum copied verbatim so GameActivity.cpp's switch/compare-by-value
// still typechecks), extended with a settable one-shot tap mechanism: nextTap/
// nextTapX/nextTapY, consumed by wasScreenTapped() the same way nextReleased is
// consumed by wasReleased() -- a single scripted "tap" only fires once per simulated
// loop() call, matching how a real touch-release event would only be true for one frame.
//
// This is the ONLY stub that differs from the whole_run_corrupt_save precedent --
// everything else here (Button/SwipeDir enums, Labels, RowTouch, all other inert
// no-op/false-return methods) is copied verbatim since GameActivity.cpp's
// CorruptSaveNotice/loop() branch only ever calls wasReleased() and wasScreenTapped()
// (confirmed via grep of GameActivity.cpp), the rest exist purely because
// GameRenderer.h's hitTestControls(..., Button&) signature references the type.
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

  // Test-only knob: set to the button a scripted frame should report as released,
  // then call the real loop(). Auto-clears after being read once.
  static constexpr Button kNone = static_cast<Button>(0xFF);
  Button nextReleased = kNone;

  // Test-only knob: set to simulate a single screen-tap frame at (nextTapX, nextTapY),
  // then call the real loop(). Auto-clears after being read once by wasScreenTapped(),
  // same one-shot-edge-event semantics as nextReleased above.
  bool nextTap = false;
  int nextTapX = 0;
  int nextTapY = 0;

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
  bool wasScreenTapped(int& x, int& y) {
    if (nextTap) {
      x = nextTapX;
      y = nextTapY;
      nextTap = false;
      return true;
    }
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
