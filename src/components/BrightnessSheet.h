#pragma once

class GfxRenderer;
class MappedInputManager;

// Touch-first control centre: a card pinned to the top of the screen, opened by
// the existing edge gesture or a top-level status-bar tap.
// Unlike FrontlightActivity, this is NOT an Activity — it draws directly into the
// live framebuffer over whatever's already on screen (e.g. a reader page) and pushes
// only its own band to the panel via GfxRenderer::displayWindow(), so opening/adjusting
// it never disturbs (or requires redrawing) the rest of the screen. Dismissal falls
// back to a normal full-screen ActivityManager::requestUpdate(), since restoring
// whatever was under the band is exactly what a full repaint already does.
//
// Hosts Brightness (1..100) and Warm/Cool Balance (0..100), each with exact
// one-percent trim and a continuous touch track. The brightness lamp is the
// separate off control. Four touch tiles expose Night Mode, Refresh Screen,
// reading orientation and Touch Reader Controls using existing UI vocabulary.
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
  unsigned char brightness = 1;
  unsigned char warmth = 0;
  bool lightOn = false;

  int sheetHeight() const;
  void setBrightness(unsigned char value);
  void setWarmth(unsigned char value);
  void trimBrightness(int delta);
  void trimWarmth(int delta);
  void toggleLight();
  void activateTile(int index);
  void close();
  void draw(bool cleanRefresh = false) const;
};
