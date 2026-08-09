#include "FrontlightPinDiagnosticActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "Frontlight.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void FrontlightPinDiagnosticActivity::onEnter() {
  Activity::onEnter();
  enterPhase(PHASE_COOL);
}

void FrontlightPinDiagnosticActivity::onExit() {
  Activity::onExit();
  restoreSavedFrontlight();
}

void FrontlightPinDiagnosticActivity::enterPhase(State newState) {
  state = newState;
  switch (state) {
    case PHASE_COOL:
      // Isolate the cool channel: warmPercent=0 routes the whole brightness to cool.
      frontlightManager.setColorTemperature(0);
      frontlightManager.setBrightness(100);
      break;
    case PHASE_WARM:
      // Isolate the warm channel: warmPercent=100 routes the whole brightness to warm.
      frontlightManager.setColorTemperature(100);
      frontlightManager.setBrightness(100);
      break;
    case DONE:
      restoreSavedFrontlight();
      break;
  }
  requestUpdate();
}

void FrontlightPinDiagnosticActivity::restoreSavedFrontlight() {
  frontlightManager.setColorTemperature(SETTINGS.frontlightWarmPercent);
  frontlightManager.setBrightness(SETTINGS.frontlightBrightness);
}

void FrontlightPinDiagnosticActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  int x = 0;
  int y = 0;
  const bool advance = mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y);
  if (!advance) return;

  switch (state) {
    case PHASE_COOL:
      enterPhase(PHASE_WARM);
      return;
    case PHASE_WARM:
      enterPhase(DONE);
      return;
    case DONE:
      finish();
      return;
  }
}

void FrontlightPinDiagnosticActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_FRONTLIGHT_DIAG_TITLE));

  const int midY = pageHeight / 2;

  switch (state) {
    case PHASE_COOL:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 30, tr(STR_FRONTLIGHT_DIAG_PHASE_COOL), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_FRONTLIGHT_DIAG_HINT));
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 24,
                                 frontlightManager.coolChannelAttachOk() ? tr(STR_FRONTLIGHT_DIAG_ATTACH_OK)
                                                                         : tr(STR_FRONTLIGHT_DIAG_ATTACH_FAILED));
      break;
    case PHASE_WARM:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 30, tr(STR_FRONTLIGHT_DIAG_PHASE_WARM), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY, tr(STR_FRONTLIGHT_DIAG_HINT));
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 24,
                                 frontlightManager.warmChannelAttachOk() ? tr(STR_FRONTLIGHT_DIAG_ATTACH_OK)
                                                                         : tr(STR_FRONTLIGHT_DIAG_ATTACH_FAILED));
      break;
    case DONE:
      renderer.drawCenteredText(UI_12_FONT_ID, midY - 20, tr(STR_FRONTLIGHT_DIAG_DONE), true, EpdFontFamily::BOLD);
      renderer.drawCenteredText(UI_10_FONT_ID, midY + 10, tr(STR_FRONTLIGHT_DIAG_DONE_HINT));
      break;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
