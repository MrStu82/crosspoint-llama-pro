#include "FrontlightActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <memory>

#include "CrossPointSettings.h"
#include "Frontlight.h"
#include "MappedInputManager.h"
#include "activities/settings/FrontlightPinDiagnosticActivity.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// TURN_OFF sits before the optional WARM_COOL/PIN_DIAGNOSTIC rows so it's always
// visible; WARM_COOL and PIN_DIAGNOSTIC are the trailing items hidden via
// visibleItemCount on single-channel boards, mirroring StatusBarSettingsActivity's
// trailing-optional-items pattern. PIN_DIAGNOSTIC is gated the same as WARM_COOL
// (hasColorTemperature()) since it only makes sense on a two-channel board.
enum MenuItem { ITEM_BRIGHTNESS = 0, ITEM_TURN_OFF, ITEM_WARM_COOL, ITEM_PIN_DIAGNOSTIC, ITEM_COUNT };

constexpr int BASE_MENU_ITEMS = ITEM_WARM_COOL;  // Items shown when there's no warm/cool channel
constexpr int FULL_MENU_ITEMS = ITEM_COUNT;      // Items shown when hasColorTemperature()

const StrId menuNames[FULL_MENU_ITEMS] = {
    StrId::STR_BRIGHTNESS,
    StrId::STR_TURN_OFF,
    StrId::STR_WARM_COOL_BALANCE,
    StrId::STR_FRONTLIGHT_PIN_DIAGNOSTIC,
};

std::string formatPercent(uint8_t percent) {
  if (percent == 0) return tr(STR_FRONTLIGHT_OFF);
  char buf[8];
  snprintf(buf, sizeof(buf), tr(STR_FRONTLIGHT_PERCENT_FORMAT), percent);
  return buf;
}

std::string formatWarmCool(uint8_t warmPercent) {
  if (warmPercent == 0) return tr(STR_FRONTLIGHT_FULL_COOL);
  if (warmPercent >= 100) return tr(STR_FRONTLIGHT_FULL_WARM);
  char buf[8];
  snprintf(buf, sizeof(buf), tr(STR_FRONTLIGHT_PERCENT_FORMAT), warmPercent);
  return buf;
}
}  // namespace

void FrontlightActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  visibleItemCount = frontlightManager.hasColorTemperature() ? FULL_MENU_ITEMS : BASE_MENU_ITEMS;

  if (SETTINGS.frontlightBrightness > CrossPointSettings::FRONTLIGHT_MAX) {
    SETTINGS.frontlightBrightness = CrossPointSettings::FRONTLIGHT_MAX;
  }
  if (SETTINGS.frontlightWarmPercent > CrossPointSettings::FRONTLIGHT_MAX) {
    SETTINGS.frontlightWarmPercent = CrossPointSettings::FRONTLIGHT_MAX;
  }

  requestUpdate();
}

void FrontlightActivity::onExit() { Activity::onExit(); }

void FrontlightActivity::loop() {
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  switch (handleListTouch(selectedIndex, visibleItemCount, contentTop, contentHeight, false)) {
    case ListTouchResult::Activated:
      handleSelection();
      requestUpdate();
      return;
    case ListTouchResult::Consumed:
      return;
    case ListTouchResult::None:
      break;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, visibleItemCount);
    requestUpdate();
  });
}

void FrontlightActivity::handleSelection() {
  switch (selectedIndex) {
    case ITEM_BRIGHTNESS:
      openBrightnessPicker();
      return;
    case ITEM_WARM_COOL:
      openWarmCoolPicker();
      return;
    case ITEM_PIN_DIAGNOSTIC:
      openPinDiagnostic();
      return;
    case ITEM_TURN_OFF:
      // The explicit way back to zero: writes through the same SDK abstraction as
      // every other control here, and persists so a reboot doesn't resurrect the light.
      SETTINGS.frontlightBrightness = 0;
      frontlightManager.setBrightness(0);
      SETTINGS.saveToFile();
      requestUpdate();
      return;
    default:
      return;
  }
}

void FrontlightActivity::openBrightnessPicker() {
  startActivityForResult(
      std::make_unique<IntervalSelectionActivity>(renderer, mappedInput, "FrontlightBrightnessInterval",
                                                   StrId::STR_BRIGHTNESS, SETTINGS.frontlightBrightness,
                                                   CrossPointSettings::FRONTLIGHT_MIN, CrossPointSettings::FRONTLIGHT_MAX,
                                                   CrossPointSettings::FRONTLIGHT_STEP, CrossPointSettings::FRONTLIGHT_STEP,
                                                   StrId::STR_FRONTLIGHT_PERCENT_FORMAT),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          SETTINGS.frontlightBrightness = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
          frontlightManager.setBrightness(SETTINGS.frontlightBrightness);
          SETTINGS.saveToFile();
        }
        requestUpdate();
      });
}

void FrontlightActivity::openWarmCoolPicker() {
  startActivityForResult(
      std::make_unique<IntervalSelectionActivity>(
          renderer, mappedInput, "FrontlightWarmCoolInterval", StrId::STR_WARM_COOL_BALANCE,
          SETTINGS.frontlightWarmPercent, CrossPointSettings::FRONTLIGHT_MIN, CrossPointSettings::FRONTLIGHT_MAX,
          CrossPointSettings::FRONTLIGHT_STEP, CrossPointSettings::FRONTLIGHT_STEP, StrId::STR_FRONTLIGHT_PERCENT_FORMAT),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          SETTINGS.frontlightWarmPercent = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
          frontlightManager.setColorTemperature(SETTINGS.frontlightWarmPercent);
          SETTINGS.saveToFile();
        }
        requestUpdate();
      });
}

void FrontlightActivity::openPinDiagnostic() {
  startActivityForResult(std::make_unique<FrontlightPinDiagnosticActivity>(renderer, mappedInput), nullptr);
}

void FrontlightActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();

  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FRONTLIGHT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, visibleItemCount, static_cast<int>(selectedIndex),
      [](int index) { return std::string(I18N.get(menuNames[index])); }, nullptr, nullptr,
      [](int index) -> std::string {
        switch (index) {
          case ITEM_BRIGHTNESS:
            return formatPercent(SETTINGS.frontlightBrightness);
          case ITEM_TURN_OFF:
            return "";
          case ITEM_WARM_COOL:
            return formatWarmCool(SETTINGS.frontlightWarmPercent);
          case ITEM_PIN_DIAGNOSTIC:
            return "";
          default:
            return "";
        }
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
