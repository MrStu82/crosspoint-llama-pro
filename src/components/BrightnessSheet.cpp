#include "components/BrightnessSheet.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "Frontlight.h"
#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "activities/reader/ReaderUtils.h"
#include "components/ControlCenterModel.h"
#include "components/DrawerChrome.h"
#include "fontIds.h"

namespace {
using ControlCenterModel::Layout;
using ControlCenterModel::SliderLayout;

bool hit(const ControlCenterModel::Rect& rect, const int x, const int y) {
  return rect.width > 0 && rect.height > 0 && rect.contains(x, y);
}

void drawCentered(const GfxRenderer& renderer, const ControlCenterModel::Rect& rect, const char* text,
                  const bool black = true,
                  const bool bold = false) {
  const auto style = bold ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
  const int width = renderer.getTextWidth(UI_12_FONT_ID, text, style);
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(UI_12_FONT_ID, rect.x + (rect.width - width) / 2, rect.y + (rect.height - lineHeight) / 2, text,
                    black, style);
}

void drawCaption(const GfxRenderer& renderer, const ControlCenterModel::Rect& rect, const char* label,
                 const char* value) {
  renderer.drawText(UI_12_FONT_ID, rect.x, rect.y, label, true, EpdFontFamily::BOLD);
  const int valueWidth = renderer.getTextWidth(UI_12_FONT_ID, value, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, rect.x + rect.width - valueWidth, rect.y, value, true, EpdFontFamily::BOLD);
}

void drawTrack(const GfxRenderer& renderer, const ControlCenterModel::Rect& track, const int value,
               const int minimum) {
  renderer.drawRect(track.x, track.y, track.width, track.height, 2, true);
  const int cy = track.y + track.height / 2;
  renderer.fillRect(track.x + 14, cy - 1, std::max(1, track.width - 28), 2, true);
  const int knob = ControlCenterModel::knobX(
      value, ControlCenterModel::Rect{track.x + 14, track.y, std::max(1, track.width - 28), track.height}, minimum);
  renderer.fillRect(knob - 6, cy - 10, 12, 20, false);
  renderer.drawRect(knob - 6, cy - 10, 12, 20, 2, true);
}

void drawSlider(const GfxRenderer& renderer, const SliderLayout& slider, const int value, const int minimum,
                const bool withToggle, const bool lightOn) {
  renderer.drawRect(slider.minus.x, slider.minus.y, slider.minus.width, slider.minus.height, 2, true);
  drawCentered(renderer, slider.minus, "-", true, true);
  drawTrack(renderer, slider.track, value, minimum);
  renderer.drawRect(slider.plus.x, slider.plus.y, slider.plus.width, slider.plus.height, 2, true);
  drawCentered(renderer, slider.plus, "+", true, true);
  if (withToggle) {
    renderer.drawRect(slider.toggle.x, slider.toggle.y, slider.toggle.width, slider.toggle.height, 2, true);
    drawCentered(renderer, slider.toggle, lightOn ? tr(STR_FRONTLIGHT_OFF) : tr(STR_STATE_ON), true, true);
  }
}

const char* orientationLabel() {
  static constexpr StrId names[4] = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW,
                                      StrId::STR_ORIENTATION_INVERTED, StrId::STR_LANDSCAPE_CCW};
  return I18N.get(names[SETTINGS.orientation % 4]);
}
}  // namespace

int BrightnessSheet::sheetHeight() const {
  return ControlCenterModel::layout(renderer.getScreenWidth(), renderer.getScreenHeight()).sheetHeight;
}

void BrightnessSheet::open() {
  brightness = static_cast<unsigned char>(ControlCenterModel::clampBrightness(SETTINGS.frontlightBrightness));
  warmth = static_cast<unsigned char>(ControlCenterModel::clampWarmth(SETTINGS.frontlightWarmPercent));
  lightOn = SETTINGS.frontlightOn != 0 && frontlightManager.brightness() > 0;
  open_ = true;
  dragging = DragTarget::None;
  RenderLock lock;
  draw();
}

void BrightnessSheet::close() {
  open_ = false;
  dragging = DragTarget::None;
  activityManager.requestUpdate();
}

void BrightnessSheet::setBrightness(unsigned char value) {
  value = static_cast<unsigned char>(ControlCenterModel::clampBrightness(value));
  const bool changed = value != brightness || !lightOn;
  brightness = value;
  lightOn = true;
  frontlightManager.setBrightness(value);
  if (SETTINGS.frontlightBrightness != value || SETTINGS.frontlightOn == 0) {
    SETTINGS.frontlightBrightness = value;
    SETTINGS.frontlightOn = 1;
    SETTINGS.saveToFile();
  }
  if (changed) {
    RenderLock lock;
    draw();
  }
}

void BrightnessSheet::setWarmth(unsigned char value) {
  value = static_cast<unsigned char>(ControlCenterModel::clampWarmth(value));
  if (value == warmth) return;
  warmth = value;
  frontlightManager.setColorTemperature(value);
  SETTINGS.frontlightWarmPercent = value;
  SETTINGS.saveToFile();
  RenderLock lock;
  draw();
}

void BrightnessSheet::trimBrightness(const int delta) {
  setBrightness(static_cast<unsigned char>(ControlCenterModel::clampBrightness(static_cast<int>(brightness) + delta)));
}

void BrightnessSheet::trimWarmth(const int delta) {
  setWarmth(static_cast<unsigned char>(ControlCenterModel::clampWarmth(static_cast<int>(warmth) + delta)));
}

void BrightnessSheet::toggleLight() {
  lightOn = !lightOn;
  if (lightOn)
    frontlightManager.setBrightness(brightness);
  else
    frontlightManager.off();
  SETTINGS.frontlightBrightness = brightness;
  SETTINGS.frontlightOn = lightOn ? 1 : 0;
  SETTINGS.saveToFile();
  RenderLock lock;
  draw();
}

void BrightnessSheet::activateTile(const int index) {
  switch (index) {
    case 0:
      SETTINGS.screenInverted = SETTINGS.screenInverted ? 0 : 1;
      SETTINGS.saveToFile();
      display.setInverted(SETTINGS.screenInverted != 0);
      {
        RenderLock lock;
        draw(true);
      }
      return;
    case 1:
      renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
      close();
      return;
    case 2:
      SETTINGS.orientation = static_cast<unsigned char>((SETTINGS.orientation + 1) % 4);
      SETTINGS.saveToFile();
      ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
      renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
      close();
      return;
    case 3:
      SETTINGS.touchReaderControls = SETTINGS.touchReaderControls == CrossPointSettings::TOUCH_READER_OFF
                                         ? CrossPointSettings::TOUCH_READER_ON
                                         : CrossPointSettings::TOUCH_READER_OFF;
      SETTINGS.saveToFile();
      {
        RenderLock lock;
        draw();
      }
      return;
    default:
      return;
  }
}

bool BrightnessSheet::loop() {
  if (!open_) return false;

  const Layout layout = ControlCenterModel::layout(renderer.getScreenWidth(), renderer.getScreenHeight());
  int tx = 0;
  int ty = 0;
  if (mappedInput.isScreenTouchHeld(tx, ty)) {
    if (dragging == DragTarget::Brightness || (dragging == DragTarget::None && hit(layout.brightness.track, tx, ty))) {
      dragging = DragTarget::Brightness;
      const ControlCenterModel::Rect valueTrack{layout.brightness.track.x + 14, layout.brightness.track.y,
                                                 std::max(1, layout.brightness.track.width - 28),
                                                 layout.brightness.track.height};
      setBrightness(static_cast<unsigned char>(
          ControlCenterModel::valueFromTrack(tx, valueTrack, ControlCenterModel::kBrightnessMin)));
    } else if (dragging == DragTarget::Warmth || (dragging == DragTarget::None && hit(layout.warmth.track, tx, ty))) {
      dragging = DragTarget::Warmth;
      const ControlCenterModel::Rect valueTrack{layout.warmth.track.x + 14, layout.warmth.track.y,
                                                 std::max(1, layout.warmth.track.width - 28),
                                                 layout.warmth.track.height};
      setWarmth(static_cast<unsigned char>(
          ControlCenterModel::valueFromTrack(tx, valueTrack, ControlCenterModel::kWarmthMin)));
    }
    return true;
  }
  if (dragging != DragTarget::None) {
    dragging = DragTarget::None;
    return true;
  }

  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (hit(layout.brightness.minus, tx, ty))
      trimBrightness(-1);
    else if (hit(layout.brightness.plus, tx, ty))
      trimBrightness(1);
    else if (hit(layout.brightness.toggle, tx, ty))
      toggleLight();
    else if (hit(layout.warmth.minus, tx, ty))
      trimWarmth(-1);
    else if (hit(layout.warmth.plus, tx, ty))
      trimWarmth(1);
    else {
      for (int i = 0; i < 4; ++i) {
        if (hit(layout.tiles[i], tx, ty)) {
          activateTile(i);
          return true;
        }
      }
      if (DrawerChrome::isOutsideTap(DrawerChrome::Edge::Top,
                                     ::Rect{0, 0, renderer.getScreenWidth(), layout.sheetHeight},
                                     tx, ty)) {
        close();
      }
    }
    return true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    close();
  }
  return true;
}

void BrightnessSheet::draw(const bool cleanRefresh) const {
  const int screenWidth = renderer.getScreenWidth();
  const Layout layout = ControlCenterModel::layout(screenWidth, renderer.getScreenHeight());
  DrawerChrome::clearRegion(renderer, ::Rect(0, 0, screenWidth, layout.sheetHeight));
  renderer.fillRect(0, layout.sheetHeight - 2, screenWidth, 2, true);

  char value[16];
  snprintf(value, sizeof(value), tr(STR_FRONTLIGHT_PERCENT_FORMAT), static_cast<unsigned>(brightness));
  drawCaption(renderer, layout.brightnessCaption, tr(STR_BRIGHTNESS), value);
  drawSlider(renderer, layout.brightness, brightness, ControlCenterModel::kBrightnessMin, true, lightOn);

  if (warmth == 0)
    snprintf(value, sizeof(value), "%s", tr(STR_FRONTLIGHT_FULL_COOL));
  else if (warmth == 100)
    snprintf(value, sizeof(value), "%s", tr(STR_FRONTLIGHT_FULL_WARM));
  else
    snprintf(value, sizeof(value), tr(STR_FRONTLIGHT_PERCENT_FORMAT), static_cast<unsigned>(warmth));
  drawCaption(renderer, layout.warmthCaption, tr(STR_WARM_COOL_BALANCE), value);
  drawSlider(renderer, layout.warmth, warmth, ControlCenterModel::kWarmthMin, false, lightOn);

  char touchLabel[40];
  const bool touchOn = SETTINGS.touchReaderControls != CrossPointSettings::TOUCH_READER_OFF;
  snprintf(touchLabel, sizeof(touchLabel), "%s %s", tr(STR_TOUCH_TOGGLE),
           I18N.get(touchOn ? StrId::STR_STATE_ON : StrId::STR_STATE_OFF));
  const char* labels[4] = {tr(STR_NIGHT_MODE), tr(STR_FORCE_REFRESH), orientationLabel(), touchLabel};
  for (int i = 0; i < 4; ++i) {
    const auto ink = ControlCenterModel::tileInk(i, SETTINGS.screenInverted != 0, touchOn);
    if (ink.blackFill)
      renderer.fillRect(layout.tiles[i].x, layout.tiles[i].y, layout.tiles[i].width, layout.tiles[i].height);
    renderer.drawRect(layout.tiles[i].x, layout.tiles[i].y, layout.tiles[i].width, layout.tiles[i].height, 2, true);
    drawCentered(renderer, layout.tiles[i], labels[i], ink.blackText, true);
  }

  renderer.fillRoundedRect(layout.grabber.x, layout.grabber.y, layout.grabber.width, layout.grabber.height, 2,
                           Color::Black);
  if (cleanRefresh)
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  else
    renderer.displayWindow(0, 0, screenWidth, layout.sheetHeight);
}
