#include "components/BrightnessSheet.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "CrossPointSettings.h"
#include "Frontlight.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "components/DrawerChrome.h"
#include "fontIds.h"

namespace {
constexpr int TICK_COUNT = 21;  // Spans FRONTLIGHT_MIN..MAX in FRONTLIGHT_STEP (5%) increments
constexpr int SLIDER_MARGIN_X = 20;
constexpr int SLIDER_HEIGHT = 16;
constexpr int TRIM_BTN_SIZE = 26;
constexpr int TRIM_GAP = 6;
constexpr int PRESET_BTN_WIDTH = 64;
constexpr int PRESET_BTN_HEIGHT = 26;
constexpr int PRESET_GAP = 12;

constexpr int PRESET_ROW_Y_OFFSET = 10;
constexpr int BRIGHTNESS_LABEL_Y_OFFSET = 46;
constexpr int BRIGHTNESS_SLIDER_Y_OFFSET = 72;
constexpr int WARMTH_LABEL_Y_OFFSET = 118;
constexpr int WARMTH_SLIDER_Y_OFFSET = 144;

int tickValue(int index) {
  const int range = CrossPointSettings::FRONTLIGHT_MAX - CrossPointSettings::FRONTLIGHT_MIN;
  // Rounded division so ticks land on the nearest whole percent instead of always floor.
  return CrossPointSettings::FRONTLIGHT_MIN + (range * index + (TICK_COUNT - 1) / 2) / (TICK_COUNT - 1);
}

int nearestTickIndex(int value) {
  int best = 0;
  int bestDist = INT32_MAX;
  for (int i = 0; i < TICK_COUNT; ++i) {
    const int dist = std::abs(tickValue(i) - value);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}

bool hitTest(int px, int py, int rx, int ry, int rw, int rh) {
  return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

// All the geometry the sheet needs, derived once from the screen width and band top so
// draw() (rendering) and loop() (hit-testing) can never drift apart from each other.
struct Layout {
  int sliderX;
  int sliderWidth;

  int trimMinusX;
  int trimPlusX;
  int brightnessSliderY;
  int brightnessTrimY;
  int warmthSliderY;
  int warmthTrimY;

  int presetOffX;
  int presetOnePercentX;
  int presetY;
};

Layout computeLayout(int top, int screenWidth) {
  Layout layout{};
  layout.sliderX = SLIDER_MARGIN_X + TRIM_BTN_SIZE + TRIM_GAP;
  layout.sliderWidth = screenWidth - 2 * layout.sliderX;

  layout.trimMinusX = SLIDER_MARGIN_X;
  layout.trimPlusX = screenWidth - SLIDER_MARGIN_X - TRIM_BTN_SIZE;

  layout.brightnessSliderY = top + BRIGHTNESS_SLIDER_Y_OFFSET;
  layout.brightnessTrimY = layout.brightnessSliderY - (TRIM_BTN_SIZE - SLIDER_HEIGHT) / 2;

  layout.warmthSliderY = top + WARMTH_SLIDER_Y_OFFSET;
  layout.warmthTrimY = layout.warmthSliderY - (TRIM_BTN_SIZE - SLIDER_HEIGHT) / 2;

  const int presetPairWidth = 2 * PRESET_BTN_WIDTH + PRESET_GAP;
  layout.presetOffX = (screenWidth - presetPairWidth) / 2;
  layout.presetOnePercentX = layout.presetOffX + PRESET_BTN_WIDTH + PRESET_GAP;
  layout.presetY = top + PRESET_ROW_Y_OFFSET;

  return layout;
}

// Same centering idiom already used for square buttons elsewhere (e.g.
// MinesweeperActivity/SudokuActivity's `y + h / 2 - 6` baseline nudge for UI_10_FONT_ID).
void drawCenteredLabel(const GfxRenderer& renderer, int x, int y, int w, int h, const char* text) {
  const int tw = renderer.getTextWidth(UI_10_FONT_ID, text);
  renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, text, true);
}

void drawSlider(const GfxRenderer& renderer, const Layout& layout, int sliderY, int trimY, uint8_t value) {
  renderer.drawRect(layout.trimMinusX, trimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE);
  drawCenteredLabel(renderer, layout.trimMinusX, trimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE, "-");
  renderer.drawRect(layout.trimPlusX, trimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE);
  drawCenteredLabel(renderer, layout.trimPlusX, trimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE, "+");

  renderer.drawRect(layout.sliderX, sliderY, layout.sliderWidth, SLIDER_HEIGHT);

  const int selectedIdx = nearestTickIndex(value);
  for (int i = 0; i < TICK_COUNT; ++i) {
    const int tickX = layout.sliderX + layout.sliderWidth * i / (TICK_COUNT - 1);
    renderer.drawLine(tickX, sliderY - 6, tickX, sliderY, true);
    if (i == selectedIdx) {
      renderer.fillRect(tickX - 3, sliderY - 4, 6, SLIDER_HEIGHT + 8, true);
    }
  }
}
}  // namespace

int BrightnessSheet::bandTop() const { return renderer.getScreenHeight() - SHEET_HEIGHT; }

void BrightnessSheet::open() {
  open_ = true;
  dragging = DragTarget::None;
  RenderLock lock;
  draw();
}

void BrightnessSheet::close() {
  open_ = false;
  dragging = DragTarget::None;
  // Only a full repaint knows how to correctly restore whatever the band was
  // overlaying (reader page, home screen, etc.) — the sheet never drew anything
  // outside its own band, so this is the only correct way to reconstruct the rest.
  activityManager.requestUpdate();
}

void BrightnessSheet::setBrightness(unsigned char value) {
  value = std::clamp<unsigned char>(value, CrossPointSettings::FRONTLIGHT_MIN, CrossPointSettings::FRONTLIGHT_MAX);
  if (value == SETTINGS.frontlightBrightness) return;
  SETTINGS.frontlightBrightness = value;
  frontlightManager.setBrightness(value);
  SETTINGS.saveToFile();
  RenderLock lock;
  draw();
}

void BrightnessSheet::setWarmth(unsigned char value) {
  value = std::clamp<unsigned char>(value, CrossPointSettings::FRONTLIGHT_MIN, CrossPointSettings::FRONTLIGHT_MAX);
  if (value == SETTINGS.frontlightWarmPercent) return;
  SETTINGS.frontlightWarmPercent = value;
  frontlightManager.setColorTemperature(value);
  SETTINGS.saveToFile();
  RenderLock lock;
  draw();
}

void BrightnessSheet::trimBrightness(int delta) {
  const int next = static_cast<int>(SETTINGS.frontlightBrightness) + delta;
  setBrightness(static_cast<unsigned char>(std::clamp(next, static_cast<int>(CrossPointSettings::FRONTLIGHT_MIN),
                                                        static_cast<int>(CrossPointSettings::FRONTLIGHT_MAX))));
}

void BrightnessSheet::trimWarmth(int delta) {
  const int next = static_cast<int>(SETTINGS.frontlightWarmPercent) + delta;
  setWarmth(static_cast<unsigned char>(std::clamp(next, static_cast<int>(CrossPointSettings::FRONTLIGHT_MIN),
                                                    static_cast<int>(CrossPointSettings::FRONTLIGHT_MAX))));
}

bool BrightnessSheet::loop() {
  if (!open_) return false;

  const int top = bandTop();
  const int screenWidth = renderer.getScreenWidth();
  const Layout layout = computeLayout(top, screenWidth);

  int tx = 0;
  int ty = 0;
  if (mappedInput.isScreenTouchHeld(tx, ty)) {
    const bool onBrightnessTrack =
        ty >= layout.brightnessSliderY - 24 && ty < layout.brightnessSliderY + SLIDER_HEIGHT + 24;
    const bool onWarmthTrack = ty >= layout.warmthSliderY - 24 && ty < layout.warmthSliderY + SLIDER_HEIGHT + 24;

    if (dragging == DragTarget::Brightness || (dragging == DragTarget::None && onBrightnessTrack)) {
      dragging = DragTarget::Brightness;
      const int idx =
          std::clamp((tx - layout.sliderX) * (TICK_COUNT - 1) / std::max(1, layout.sliderWidth - 1), 0, TICK_COUNT - 1);
      setBrightness(static_cast<unsigned char>(tickValue(idx)));
    } else if (dragging == DragTarget::Warmth || (dragging == DragTarget::None && onWarmthTrack)) {
      dragging = DragTarget::Warmth;
      const int idx =
          std::clamp((tx - layout.sliderX) * (TICK_COUNT - 1) / std::max(1, layout.sliderWidth - 1), 0, TICK_COUNT - 1);
      setWarmth(static_cast<unsigned char>(tickValue(idx)));
    }
    return true;  // Sheet owns every touch-and-hold frame for its whole lifetime.
  }
  if (dragging != DragTarget::None) {
    dragging = DragTarget::None;
    return true;  // Swallow the release frame of a drag.
  }

  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (hitTest(tx, ty, layout.presetOffX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT)) {
      setBrightness(0);
    } else if (hitTest(tx, ty, layout.presetOnePercentX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT)) {
      setBrightness(1);
    } else if (hitTest(tx, ty, layout.trimMinusX, layout.brightnessTrimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE)) {
      trimBrightness(-1);
    } else if (hitTest(tx, ty, layout.trimPlusX, layout.brightnessTrimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE)) {
      trimBrightness(1);
    } else if (hitTest(tx, ty, layout.trimMinusX, layout.warmthTrimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE)) {
      trimWarmth(-1);
    } else if (hitTest(tx, ty, layout.trimPlusX, layout.warmthTrimY, TRIM_BTN_SIZE, TRIM_BTN_SIZE)) {
      trimWarmth(1);
    } else if (DrawerChrome::isOutsideTap(DrawerChrome::Edge::Bottom, Rect(0, top, screenWidth, SHEET_HEIGHT), tx,
                                           ty)) {
      close();
    }
    // A tap inside the band that hit nothing (e.g. between ticks) is absorbed
    // silently rather than passed through to whatever's underneath.
    return true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    close();
    return true;
  }

  return true;  // Sheet is open: own input this frame even with no event.
}

void BrightnessSheet::draw() const {
  const int top = bandTop();
  const int screenWidth = renderer.getScreenWidth();
  const Layout layout = computeLayout(top, screenWidth);

  DrawerChrome::clearRegion(renderer, Rect(0, top, screenWidth, SHEET_HEIGHT));
  renderer.drawLine(0, top, screenWidth, top, true);

  // Presets
  renderer.drawRect(layout.presetOffX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT);
  drawCenteredLabel(renderer, layout.presetOffX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT,
                     tr(STR_FRONTLIGHT_OFF));

  char onePercent[8];
  snprintf(onePercent, sizeof(onePercent), tr(STR_FRONTLIGHT_PERCENT_FORMAT), 1);
  renderer.drawRect(layout.presetOnePercentX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT);
  drawCenteredLabel(renderer, layout.presetOnePercentX, layout.presetY, PRESET_BTN_WIDTH, PRESET_BTN_HEIGHT, onePercent);

  // Brightness
  const uint8_t brightness = SETTINGS.frontlightBrightness;
  char brightnessValue[8];
  if (brightness == 0) {
    snprintf(brightnessValue, sizeof(brightnessValue), "%s", tr(STR_FRONTLIGHT_OFF));
  } else {
    snprintf(brightnessValue, sizeof(brightnessValue), tr(STR_FRONTLIGHT_PERCENT_FORMAT), brightness);
  }
  char brightnessTitle[40];
  snprintf(brightnessTitle, sizeof(brightnessTitle), "%s: %s", tr(STR_BRIGHTNESS), brightnessValue);
  renderer.drawCenteredText(UI_12_FONT_ID, top + BRIGHTNESS_LABEL_Y_OFFSET, brightnessTitle, true, EpdFontFamily::BOLD);

  drawSlider(renderer, layout, layout.brightnessSliderY, layout.brightnessTrimY, brightness);

  // Warmth
  const uint8_t warmth = SETTINGS.frontlightWarmPercent;
  char warmthValue[16];
  if (warmth == 0) {
    snprintf(warmthValue, sizeof(warmthValue), "%s", tr(STR_FRONTLIGHT_FULL_COOL));
  } else if (warmth >= 100) {
    snprintf(warmthValue, sizeof(warmthValue), "%s", tr(STR_FRONTLIGHT_FULL_WARM));
  } else {
    snprintf(warmthValue, sizeof(warmthValue), tr(STR_FRONTLIGHT_PERCENT_FORMAT), warmth);
  }
  char warmthTitle[48];
  snprintf(warmthTitle, sizeof(warmthTitle), "%s: %s", tr(STR_WARM_COOL_BALANCE), warmthValue);
  renderer.drawCenteredText(UI_12_FONT_ID, top + WARMTH_LABEL_Y_OFFSET, warmthTitle, true, EpdFontFamily::BOLD);

  drawSlider(renderer, layout, layout.warmthSliderY, layout.warmthTrimY, warmth);

  renderer.displayWindow(0, top, screenWidth, SHEET_HEIGHT);
}
