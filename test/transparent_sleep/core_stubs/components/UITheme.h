#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

struct ThemeMetrics {
  int popupFrameThickness = 1;
  float popupTopOffsetRatio = 0.25f;
  int popupMarginY = 2;
};
class UITheme {
 public:
  static UITheme& getInstance() {
    static UITheme theme;
    return theme;
  }
  const ThemeMetrics& getMetrics() const { return metrics; }
  ThemeMetrics metrics;
};
struct GuiStub {
  void drawPopup(GfxRenderer&, const char*) { ++popupCalls; }
  int popupCalls = 0;
};
inline GuiStub GUI;
