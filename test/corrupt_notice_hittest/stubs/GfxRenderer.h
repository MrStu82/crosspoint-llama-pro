#pragma once
// Shadow of lib/GfxRenderer/GfxRenderer.h. Method surface trimmed to exactly what
// GameRenderer.cpp/UITheme.cpp/BaseTheme.cpp actually call (confirmed via grep across all
// three) -- everything else in the real class (fonts, grayscale, strip targets, framebuffer
// snapshotting) is unreached by this harness's tested code paths and omitted. Signatures below
// were copied verbatim from the real lib/GfxRenderer/GfxRenderer.h (read directly this
// session) after two prior stub mismatches surfaced as real compile errors: the Color enum is
// declared at FILE scope (not nested in the class) immediately before the class, and
// Orientation is a nested UNSCOPED enum (not enum class) with different enumerator names than
// this stub originally guessed. drawRoundedRect/fillRoundedRect overload shapes similarly
// corrected against the real 7-arg / per-corner-bool call sites.
#include <cstdint>
#include <string>
#include <vector>

#include <EpdFontFamily.h>

#include "HalDisplay.h"

// Matches the real file-scope declaration in lib/GfxRenderer/GfxRenderer.h exactly.
enum Color : uint8_t { Clear = 0x00, White = 0x01, LightGray = 0x05, DarkGray = 0x0A, Black = 0x10 };

class GfxRenderer {
 public:
  // Matches the real nested, unscoped declaration exactly (not enum class, not PORTRAIT/LANDSCAPE).
  enum Orientation {
    Portrait,
    LandscapeClockwise,
    PortraitInverted,
    LandscapeCounterClockwise
  };

  void clearScreen(uint8_t = 0xFF) const {}
  void drawPixel(int, int, bool = true) const {}
  void displayBuffer(HalDisplay::RefreshMode = HalDisplay::FAST_REFRESH) const {}
  void displayBufferGhostGuard(int&, int, HalDisplay::RefreshMode = HalDisplay::FAST_REFRESH) const {}
  void displayWindow(int, int, int, int, bool = false) const {}

  void drawCenteredText(int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}

  void drawLine(int, int, int, int, bool = true) const {}
  void drawLine(int, int, int, int, int, bool) const {}

  void drawRect(int, int, int, int, bool = true) const {}
  void drawRect(int, int, int, int, int, bool) const {}

  void drawRoundedRect(int, int, int, int, int, int, bool) const {}
  void drawRoundedRect(int, int, int, int, int, int, bool, bool, bool, bool, bool) const {}

  void drawText(int, int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {}

  void fillPolygon(const int*, const int*, int, bool = true) const {}
  void fillRect(int, int, int, int, bool = true) const {}
  void fillRectDither(int, int, int, int, Color) const {}

  void fillRoundedRect(int, int, int, int, int, Color) const {}
  void fillRoundedRect(int, int, int, int, int, bool, bool, bool, bool, Color) const {}

  void drawBitmap(int, int, int, int, int, float = 0, float = 0) const {}

  void getOrientedViewableTRBL(int* outTop, int* outRight, int* outBottom, int* outLeft) const {
    if (outTop) *outTop = 0;
    if (outRight) *outRight = 0;
    if (outBottom) *outBottom = 0;
    if (outLeft) *outLeft = 0;
  }

  int getLineHeight(int) const { return 20; }

  int getScreenHeight() const { return 800; }
  int getScreenWidth() const { return 480; }
  int getTextHeight(int) const { return 20; }
  void setOrientation(Orientation) {}
  Orientation getOrientation() const { return Portrait; }

  int getTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return text ? static_cast<int>(std::string(text).size()) * 8 : 0;
  }

  std::string truncatedText(int, const char* text, int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return text ? std::string(text) : std::string();
  }

  std::vector<std::string> wrappedText(int, const char* text, int, int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    std::vector<std::string> lines;
    if (text) lines.push_back(text);
    return lines;
  }
};
