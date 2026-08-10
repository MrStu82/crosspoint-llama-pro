#pragma once

class GfxRenderer;
class MappedInputManager;

// Quick brightness/warmth sheet: a band pinned to the bottom of the screen, opened by
// a bottom-edge upward swipe (MappedInputManager::wasBrightnessSheetGesture()).
// Unlike FrontlightActivity, this is NOT an Activity — it draws directly into the
// live framebuffer over whatever's already on screen (e.g. a reader page) and pushes
// only its own band to the panel via GfxRenderer::displayWindow(), so opening/adjusting
// it never disturbs (or requires redrawing) the rest of the screen. Dismissal falls
// back to a normal full-screen ActivityManager::requestUpdate(), since restoring
// whatever was under the band is exactly what a full repaint already does.
//
// Hosts two independent controls: a BRIGHTNESS slider (0-100%, backed by
// SETTINGS.frontlightBrightness) and a WARMTH slider (0=cool..100=warm, backed by
// SETTINGS.frontlightWarmPercent). Each slider snaps to 5% ticks on drag and has
// flanking -1/+1 trim buttons for single-percent adjustment. Two presets ("Off",
// "1%") sit above the brightness slider for one-tap access to the two most common
// low-light values.
class BrightnessSheet {
 public:
  BrightnessSheet(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : renderer(renderer), mappedInput(mappedInput) {}

  bool isOpen() const { return open_; }

  // Opens the sheet and draws it immediately (own RenderLock + windowed push).
  void open();

  // Handles all input while open: tap/drag on either slider snaps its value to the
  // nearest 5% tick; the flanking -1/+1 buttons trim by a single percent; the "Off"
  // and "1%" buttons jump brightness directly to that value; a tap above the band, or
  // Back/Confirm, closes it. Always returns true while open, since the sheet owns
  // input for the whole duration it's on screen — the caller (ActivityManager::loop())
  // must skip dispatching to currentActivity for any frame this returns true.
  bool loop();

 private:
  enum class DragTarget { None, Brightness, Warmth };

  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  bool open_ = false;
  DragTarget dragging = DragTarget::None;

  static constexpr int SHEET_HEIGHT = 200;

  int bandTop() const;
  void setBrightness(unsigned char value);
  void setWarmth(unsigned char value);
  void trimBrightness(int delta);
  void trimWarmth(int delta);
  void close();
  void draw() const;
};
