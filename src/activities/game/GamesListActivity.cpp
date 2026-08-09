#include "GamesListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "activities/ActivityManager.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GamesListActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void GamesListActivity::onExit() { Activity::onExit(); }

void GamesListActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight =
      renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  switch (handleListTouch(selectedIndex, kItemCount, contentTop, contentHeight, false)) {
    case ListTouchResult::Activated:
      handleSelection();
      return;
    case ListTouchResult::Consumed:
      return;
    case ListTouchResult::None:
      break;
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, kItemCount);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, kItemCount);
    requestUpdate();
  });
}

void GamesListActivity::handleSelection() {
  switch (selectedIndex) {
    case 0:
      activityManager.goToDeepMines();
      return;
    case 1:
      activityManager.goToTetris();
      return;
    case 2:
      activityManager.goToTamagotchi();
      return;
    case 3:
      activityManager.goToSolitaire();
      return;
    case 4:
      activityManager.goToMinesweeper();
      return;
    default:
      return;
  }
}

void GamesListActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_GAMES_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, kItemCount, selectedIndex, [](int index) {
        switch (index) {
          case 0:
            return std::string(tr(STR_DM_TITLE));
          case 1:
            return std::string(tr(STR_TETRIS_TITLE));
          case 2:
            return std::string(tr(STR_TAMA_TITLE));
          case 3:
            return std::string(tr(STR_SOLITAIRE_TITLE));
          case 4:
            return std::string(tr(STR_MINESWEEPER_TITLE));
          default:
            return std::string();
        }
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
