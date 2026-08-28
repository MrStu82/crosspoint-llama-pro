#pragma once

#include <Arduino.h>
#include <FontCacheManager.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

class Bitmap;

struct EpdFontFamily {
  enum Style { REGULAR, BOLD };
};

class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_LSB, GRAYSCALE_MSB };
  enum class Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  struct PixelDraw {
    int x;
    int y;
    bool state;
    RenderMode mode;
  };

  GfxRenderer(int width = 32, int height = 48) : width_(width), height_(height) {}
  int getScreenWidth() const { return width_; }
  int getScreenHeight() const { return height_; }
  int getLineHeight(int) const { return 12; }
  void setOrientation(Orientation orientation) { orientation_ = orientation; }
  Orientation getOrientation() const { return orientation_; }
  FontCacheManager* getFontCacheManager() const { return const_cast<FontCacheManager*>(&fontCache); }
  void invertScreen() { ++invertCalls; }
  void clearScreen(uint8_t = 0xFF) { ++clearCalls; }
  void drawImage(const uint8_t*, int, int, int, int) {}
  void drawCenteredText(int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) {}
  void drawBitmap(const Bitmap&, int, int, int, int, float, float) { ++drawBitmapCalls; }
  void drawPixel(int x, int y, bool state) const { pixelDraws.push_back({x, y, state, mode_}); }
  void displayBuffer(HalDisplay::RefreshMode mode = HalDisplay::FAST_REFRESH) { refreshes.push_back(mode); }
  void displayGrayscaleBase(HalDisplay::RefreshMode mode) { refreshes.push_back(mode); }
  void displayGrayBuffer() { ++grayDisplays; }
  void setRenderMode(RenderMode mode) { mode_ = mode; }
  void copyGrayscaleLsbBuffers() {}
  void copyGrayscaleMsbBuffers() {}
  size_t getRegionByteSize(int, int, int width, int height) const {
    return static_cast<size_t>((width + 7) / 8) * height;
  }
  bool copyRegionToBuffer(int, int, int, int, uint8_t*, size_t) const { return true; }
  bool copyBufferToRegion(int, int, int, int, const uint8_t*, size_t) const { return true; }

  FontCacheManager fontCache;
  mutable std::vector<PixelDraw> pixelDraws;
  std::vector<HalDisplay::RefreshMode> refreshes;
  int invertCalls = 0;
  int clearCalls = 0;
  int drawBitmapCalls = 0;
  int grayDisplays = 0;

 private:
  int width_;
  int height_;
  Orientation orientation_ = Orientation::Portrait;
  RenderMode mode_ = BW;
};

enum class BmpReaderError { Ok, Invalid };

class Bitmap {
 public:
  explicit Bitmap(HalFile& file, bool dither = false) : file_(file) { lastDither = dither; }
  BmpReaderError parseHeaders() {
    uint8_t header[30]{};
    if (!file_.seek(0) || file_.read(header, sizeof(header)) != static_cast<int>(sizeof(header)) || header[0] != 'B' ||
        header[1] != 'M') {
      return BmpReaderError::Invalid;
    }
    width_ = static_cast<int>(header[18] | (header[19] << 8));
    height_ = static_cast<int>(header[22] | (header[23] << 8));
    return width_ > 0 && height_ > 0 ? BmpReaderError::Ok : BmpReaderError::Invalid;
  }
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }
  bool hasGreyscale() const { return false; }
  void rewindToData() const { file_.seek(54); }
  static const char* errorToString(BmpReaderError error) { return error == BmpReaderError::Ok ? "ok" : "invalid"; }
  static inline bool lastDither = true;

 private:
  HalFile& file_;
  int width_ = 1;
  int height_ = 1;
};
