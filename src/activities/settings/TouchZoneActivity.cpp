#if FREEINK_DEVICE_PICCO
#include "TouchZoneActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
const char* actionLabel(const uint8_t action) {
  switch (action) {
    case CrossPointSettings::TOUCH_ZONE_PREV_PAGE:
      return "Prev page";
    case CrossPointSettings::TOUCH_ZONE_NEXT_PAGE:
      return "Next page";
    case CrossPointSettings::TOUCH_ZONE_TEXT_SETTINGS:
      return "Font menu";
    case CrossPointSettings::TOUCH_ZONE_READER_MENU:
      return "Reader menu";
    default:
      return "None";
  }
}
}  // namespace

int TouchZoneActivity::gridTop() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
}

void TouchZoneActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const int width = renderer.getScreenWidth();
  const int top = gridTop();
  const int height = renderer.getScreenHeight() - top;
  if (y < top) return;
  const int col = std::min(2, x * 3 / width);
  const int row = std::min(2, (y - top) * 3 / height);
  uint8_t& action = SETTINGS.touchZoneActions[row * 3 + col];
  action = static_cast<uint8_t>((action + 1) % CrossPointSettings::TOUCH_ZONE_ACTION_COUNT);
  SETTINGS.saveToFile();
  requestUpdate();
}

void TouchZoneActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int top = gridTop();
  const int cellW = width / 3;
  const int cellH = (renderer.getScreenHeight() - top) / 3;
  const int textOffset = (cellH - renderer.getLineHeight(UI_12_FONT_ID)) / 2;

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, "Touch Zone");
  for (int i = 0; i < CrossPointSettings::TOUCH_ZONE_CELLS; ++i) {
    const int cx = (i % 3) * cellW;
    const int cy = top + (i / 3) * cellH;
    renderer.drawRect(cx, cy, cellW, cellH);
    const char* label = actionLabel(SETTINGS.touchZoneActions[i]);
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, label);
    renderer.drawText(UI_12_FONT_ID, cx + (cellW - tw) / 2, cy + textOffset, label);
  }
  renderer.displayBuffer();
}
#endif
