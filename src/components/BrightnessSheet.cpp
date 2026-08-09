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
#include "fontIds.h"

namespace {
constexpr int TICK_COUNT = 9;  // Evenly spans CrossPointSettings::FRONTLIGHT_MIN..MAX
constexpr int SLIDER_MARGIN_X = 20;
constexpr int SLIDER_Y_OFFSET = 70;  // From the band's top edge
constexpr int SLIDER_HEIGHT = 16;

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
}  // namespace

int BrightnessSheet::bandTop() const { return renderer.getScreenHeight() - SHEET_HEIGHT; }

void BrightnessSheet::open() {
  open_ = true;
  dragging = false;
  RenderLock lock;
  draw();
}

void BrightnessSheet::close() {
  open_ = false;
  dragging = false;
  // Only a full repaint knows how to correctly restore whatever the band was
  // overlaying (reader page, home screen, etc.) — the sheet never drew anything
  // outside its own band, so this is the only correct way to reconstruct the rest.
  activityManager.requestUpdate();
}

void BrightnessSheet::setValue(unsigned char value) {
  if (value == SETTINGS.frontlightBrightness) return;
  // Same persistence pattern as FrontlightActivity::openBrightnessPicker().
  SETTINGS.frontlightBrightness = value;
  frontlightManager.setBrightness(value);
  SETTINGS.saveToFile();
  RenderLock lock;
  draw();
}

bool BrightnessSheet::loop() {
  if (!open_) return false;

  const int top = bandTop();
  const int screenWidth = renderer.getScreenWidth();
  const int sliderY = top + SLIDER_Y_OFFSET;
  const int sliderX = SLIDER_MARGIN_X;
  const int sliderWidth = screenWidth - 2 * SLIDER_MARGIN_X;

  int tx = 0;
  int ty = 0;
  if (mappedInput.isScreenTouchHeld(tx, ty)) {
    if (dragging || (ty >= sliderY - 24 && ty < sliderY + SLIDER_HEIGHT + 24)) {
      dragging = true;
      const int idx =
          std::clamp((tx - sliderX) * (TICK_COUNT - 1) / std::max(1, sliderWidth - 1), 0, TICK_COUNT - 1);
      setValue(static_cast<unsigned char>(tickValue(idx)));
    }
    return true;  // Sheet owns every touch-and-hold frame for its whole lifetime.
  }
  if (dragging) {
    dragging = false;
    return true;  // Swallow the release frame of a drag.
  }

  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (ty < top) {
      close();
    }
    // A tap inside the band that isn't a drag release (e.g. between ticks) is
    // absorbed silently rather than passed through to whatever's underneath.
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

  renderer.fillRect(0, top, screenWidth, SHEET_HEIGHT, false);
  renderer.drawLine(0, top, screenWidth, top, true);

  const uint8_t value = SETTINGS.frontlightBrightness;
  char title[24];
  if (value == 0) {
    snprintf(title, sizeof(title), "%s", tr(STR_FRONTLIGHT_OFF));
  } else {
    snprintf(title, sizeof(title), tr(STR_FRONTLIGHT_PERCENT_FORMAT), value);
  }
  renderer.drawCenteredText(UI_12_FONT_ID, top + 20, title, true, EpdFontFamily::BOLD);

  const int sliderY = top + SLIDER_Y_OFFSET;
  const int sliderX = SLIDER_MARGIN_X;
  const int sliderWidth = screenWidth - 2 * SLIDER_MARGIN_X;
  renderer.drawRect(sliderX, sliderY, sliderWidth, SLIDER_HEIGHT);

  const int selectedIdx = nearestTickIndex(value);
  for (int i = 0; i < TICK_COUNT; ++i) {
    const int tickX = sliderX + sliderWidth * i / (TICK_COUNT - 1);
    renderer.drawLine(tickX, sliderY - 6, tickX, sliderY, true);
    if (i == selectedIdx) {
      renderer.fillRect(tickX - 3, sliderY - 4, 6, SLIDER_HEIGHT + 8, true);
    }
  }

  renderer.displayWindow(0, top, screenWidth, SHEET_HEIGHT);
}
