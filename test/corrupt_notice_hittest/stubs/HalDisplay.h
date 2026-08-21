#pragma once
// Shadow of lib/hal/HalDisplay.h, backed by an in-memory 1bpp framebuffer instead of the
// real EInkDisplay/SPI panel driver. Geometry is the PHYSICAL panel size (800x480, per
// FreeInkDisplay.h's DISPLAY_WIDTH/HEIGHT constants), not the logical portrait 480x800 --
// GfxRenderer::begin() reads getDisplayWidth()/Height() as the physical panelWidth/panelHeight
// it rotates logical draw coordinates into (see GfxRenderer.cpp's rotateCoordinates(), Portrait
// case: phyX=y, phyY=panelHeight-1-x). Reporting 480x800 here (an earlier mistake) fed
// GfxRenderer a panel it thought was portrait-shaped, so its own 90-degree rotation then sent
// physical coordinates outside the real physical bounds -- the "Outside range" spam. Matches
// real hardware now; the harness's PGM dump un-rotates back to logical portrait for output.
#include <cstdint>
#include <cstring>
#include <vector>

class HalDisplay {
 public:
  HalDisplay() { framebuffer.assign(BUFFER_SIZE, 0xFF); }
  ~HalDisplay() = default;

  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };

  static constexpr uint16_t DISPLAY_WIDTH = 800;
  static constexpr uint16_t DISPLAY_HEIGHT = 480;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  void begin(bool seamless = false) { (void)seamless; }

  void clearScreen(uint8_t color = 0xFF) const { std::memset(framebuffer.data(), color, framebuffer.size()); }

  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const {
    (void)imageData;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)fromProgmem;
  }
  void drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             bool fromProgmem = false) const {
    (void)imageData;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)fromProgmem;
  }

  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false) {
    (void)mode;
    (void)turnOffScreen;
  }
  void displayBufferAsync(RefreshMode mode = FAST_REFRESH) { (void)mode; }
  void waitRefreshComplete() {}
  bool supportsAsyncRefresh() const { return false; }
  void refreshDisplay(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false) {
    (void)mode;
    (void)turnOffScreen;
  }

  void deepSleep() {}

  uint8_t* getFrameBuffer() const { return framebuffer.data(); }

  uint8_t* lendFrameBufferStorage(uint32_t* sizeOut) {
    if (sizeOut) *sizeOut = 0;
    return nullptr;
  }
  void returnFrameBufferStorage() {}

  void preconditionGrayscale() {}
  void preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
  }

  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH, bool turnOffScreen = false) {
    (void)fallback;
    (void)turnOffScreen;
  }

  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
    (void)lsbBuffer;
    (void)msbBuffer;
  }
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) { (void)lsbBuffer; }
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) { (void)msbBuffer; }
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer) { (void)bwBuffer; }

  void displayGrayBuffer(bool turnOffScreen = false) { (void)turnOffScreen; }

  void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool turnOffScreen = false) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)turnOffScreen;
  }

  void writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows) {
    (void)lsbPlane;
    (void)rows;
    (void)yStart;
    (void)numRows;
  }
  bool supportsStripGrayscale() const { return false; }

  void setCustomLut(bool enabled, const unsigned char* lutData = nullptr) {
    (void)enabled;
    (void)lutData;
  }

  uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
  uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
  uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
  uint32_t getBufferSize() const { return BUFFER_SIZE; }

  // Test-only: harness reads this directly to dump the final image.
  mutable std::vector<uint8_t> framebuffer;
};

inline HalDisplay display;
