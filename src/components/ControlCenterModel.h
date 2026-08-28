#pragma once

#include <algorithm>
#include <cstdint>

// Allocation-free geometry and value mapping shared by the control-centre
// renderer, hit testing and native regression tests. Keeping this arithmetic
// here prevents the touch surface from drifting away from what is drawn.
namespace ControlCenterModel {

constexpr int kBrightnessMin = 1;
constexpr int kWarmthMin = 0;
constexpr int kValueMax = 100;

constexpr int kPortraitSheetHeight = 516;
constexpr int kSideMargin = 16;
constexpr int kTopPadding = 18;
constexpr int kCaptionHeight = 24;
constexpr int kControlHeight = 56;
constexpr int kSliderBlockHeight = 116;
constexpr int kTileHeight = 84;
constexpr int kTileGap = 16;
constexpr int kGrabberWidth = 56;
constexpr int kGrabberHeight = 5;

struct Rect {
  int x;
  int y;
  int width;
  int height;

  constexpr bool contains(const int px, const int py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};

struct SliderLayout {
  Rect minus;
  Rect track;
  Rect plus;
  Rect toggle;
};

struct Layout {
  int sheetHeight;
  Rect brightnessCaption;
  SliderLayout brightness;
  Rect warmthCaption;
  SliderLayout warmth;
  Rect tiles[4];
  Rect grabber;
};

constexpr int clampBrightness(const int value) {
  return value < kBrightnessMin ? kBrightnessMin : (value > kValueMax ? kValueMax : value);
}

constexpr int clampWarmth(const int value) {
  return value < kWarmthMin ? kWarmthMin : (value > kValueMax ? kValueMax : value);
}

struct TileInk {
  bool blackFill;
  bool blackText;
};

constexpr TileInk tileInk(const int index, const bool nightModeOn, const bool touchOn) {
  const bool selected = index == 0 ? nightModeOn : (index == 3 ? touchOn : false);
  return {selected, !selected};
}

struct FrontlightState {
  uint8_t brightness;
  uint8_t on;
};

constexpr FrontlightState migrateFrontlightState(const bool hasExplicitOnState, const uint8_t legacyBrightness,
                                                  const uint8_t loadedBrightness, const uint8_t loadedOn) {
  return {static_cast<uint8_t>(clampBrightness(loadedBrightness)),
          static_cast<uint8_t>(hasExplicitOnState ? (loadedOn ? 1 : 0) : (legacyBrightness > 0 ? 1 : 0))};
}

inline int valueFromTrack(const int x, const Rect& track, const int minimum) {
  const int span = std::max(1, track.width - 1);
  const int offset = std::clamp(x - track.x, 0, span);
  const int range = kValueMax - minimum;
  return minimum + (offset * range + span / 2) / span;
}

inline int knobX(const int value, const Rect& track, const int minimum) {
  const int clamped = std::clamp(value, minimum, kValueMax);
  const int range = std::max(1, kValueMax - minimum);
  return track.x + (clamped - minimum) * std::max(1, track.width - 1) / range;
}

inline Layout layout(const int screenWidth, const int screenHeight) {
  Layout out{};
  out.sheetHeight = std::min(kPortraitSheetHeight, screenHeight);
  const int contentWidth = std::max(0, screenWidth - 2 * kSideMargin);
  const int y0 = kTopPadding;

  out.brightnessCaption = {kSideMargin, y0, contentWidth, kCaptionHeight};
  const int brightnessY = y0 + kCaptionHeight;
  const int brightnessTrackWidth = std::max(1, contentWidth - 3 * kControlHeight);
  out.brightness.minus = {kSideMargin, brightnessY, kControlHeight, kControlHeight};
  out.brightness.track = {out.brightness.minus.x + kControlHeight, brightnessY, brightnessTrackWidth, kControlHeight};
  out.brightness.plus = {out.brightness.track.x + brightnessTrackWidth, brightnessY, kControlHeight, kControlHeight};
  out.brightness.toggle = {out.brightness.plus.x + kControlHeight, brightnessY, kControlHeight, kControlHeight};

  const int warmthCaptionY = y0 + kSliderBlockHeight;
  out.warmthCaption = {kSideMargin, warmthCaptionY, contentWidth, kCaptionHeight};
  const int warmthY = warmthCaptionY + kCaptionHeight;
  const int warmthTrackWidth = std::max(1, contentWidth - 2 * kControlHeight);
  out.warmth.minus = {kSideMargin, warmthY, kControlHeight, kControlHeight};
  out.warmth.track = {out.warmth.minus.x + kControlHeight, warmthY, warmthTrackWidth, kControlHeight};
  out.warmth.plus = {out.warmth.track.x + warmthTrackWidth, warmthY, kControlHeight, kControlHeight};
  out.warmth.toggle = {0, 0, 0, 0};

  const int tileTop = y0 + 2 * kSliderBlockHeight;
  const int tileWidth = std::max(1, (contentWidth - kTileGap) / 2);
  out.tiles[0] = {kSideMargin, tileTop, tileWidth, kTileHeight};
  out.tiles[1] = {kSideMargin + tileWidth + kTileGap, tileTop, tileWidth, kTileHeight};
  out.tiles[2] = {kSideMargin, tileTop + kTileHeight + kTileGap, tileWidth, kTileHeight};
  out.tiles[3] = {kSideMargin + tileWidth + kTileGap, tileTop + kTileHeight + kTileGap, tileWidth, kTileHeight};

  out.grabber = {(screenWidth - kGrabberWidth) / 2, out.sheetHeight - 16, kGrabberWidth, kGrabberHeight};
  return out;
}

}  // namespace ControlCenterModel
