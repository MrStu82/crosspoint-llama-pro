#pragma once

#include <BidiUtils.h>
#include <EpdFontFamily.h>

#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

class GfxRenderer {
 public:
  bool isFontCacheScanning() const { return false; }
  bool isSdCardFont(int) const { return false; }
  void ensureSdCardFontReady(int, const std::deque<std::string>&, bool, uint8_t) const {}
  int getTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR,
                   BidiUtils::BidiBaseDir = BidiUtils::BidiBaseDir::AUTO) const {
    return getTextAdvanceX(0, text, EpdFontFamily::REGULAR);
  }
  void drawText(int, int, int, const char*, bool = true, EpdFontFamily::Style = EpdFontFamily::REGULAR,
                BidiUtils::BidiBaseDir = BidiUtils::BidiBaseDir::AUTO) const {}
  int getSpaceWidth(int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const { return 4; }
  int getSpaceAdvance(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 4; }
  int getKerning(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }
  int getTextAdvanceX(int, const char* text, EpdFontFamily::Style) const {
    return static_cast<int>(std::strlen(text)) * 6;
  }
  int getFontAscenderSize(int) const { return 9; }
  int getLineHeight(int) const { return 12; }
  int getLineHeight(int, float compression) const { return static_cast<int>(12 * compression); }
  void drawLine(int, int, int, int, bool = true) const {}
  void drawLine(int, int, int, int, int, bool) const {}
  void fillRect(int, int, int, int, bool = true) const {}
};
