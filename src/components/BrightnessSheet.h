#pragma once

class GfxRenderer;
class MappedInputManager;

// Quick brightness sheet: a 160px band pinned to the bottom of the screen, opened by
// a bottom-edge upward swipe (MappedInputManager::wasBrightnessSheetGesture()).
// Unlike FrontlightActivity, this is NOT an Activity — it draws directly into the
// live framebuffer over whatever's already on screen (e.g. a reader page) and pushes
// only its own band to the panel via GfxRenderer::displayWindow(), so opening/adjusting
// it never disturbs (or requires redrawing) the rest of the screen. Dismissal falls
// back to a normal full-screen ActivityManager::requestUpdate(), since restoring
// whatever was under the band is exactly what a full repaint already does.
class BrightnessSheet {
 public:
  BrightnessSheet(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : renderer(renderer), mappedInput(mappedInput) {}

  bool isOpen() const { return open_; }

  // Opens the sheet and draws it immediately (own RenderLock + windowed push).
  void open();

  // Handles all input while open: tap/drag on the slider snaps brightness to the
  // nearest of 9 ticks (each change redraws only the band); a tap above the band, or
  // Back/Confirm, closes it. Always returns true while open, since the sheet owns
  // input for the whole duration it's on screen — the caller (ActivityManager::loop())
  // must skip dispatching to currentActivity for any frame this returns true.
  bool loop();

 private:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  bool open_ = false;
  bool dragging = false;

  static constexpr int SHEET_HEIGHT = 160;

  int bandTop() const;
  void setValue(unsigned char value);
  void close();
  void draw() const;
};
