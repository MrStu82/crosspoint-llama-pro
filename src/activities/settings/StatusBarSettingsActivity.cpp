#include "StatusBarSettingsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstring>
#include <memory>

#include "ClockOffsetActivity.h"
#include "ClockSyncActivity.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Menu items in their natural order. Clock entries are appended only when the
// DS3231 RTC is present so X4 devices don't see them at all.
enum MenuItem {
  ITEM_CHAPTER_PAGE_COUNT = 0,
  ITEM_BOOK_PROGRESS_PERCENTAGE,
  ITEM_PROGRESS_BAR,
  ITEM_PROGRESS_BAR_THICKNESS,
  ITEM_TITLE,
  ITEM_BATTERY,
  ITEM_XTC_STATUS_BAR,
  ITEM_CLOCK,             // X3 only
  ITEM_CLOCK_FORMAT,      // X3 only
  ITEM_CLOCK_UTC_OFFSET,  // X3 only, launches ClockOffsetActivity
  ITEM_CLOCK_SYNC,        // X3 only, launches ClockSyncActivity
  ITEM_COUNT
};

constexpr int BASE_MENU_ITEMS = ITEM_CLOCK;  // Items shown on every device (bottom bar)
constexpr int FULL_MENU_ITEMS = ITEM_COUNT;  // Items shown when RTC is available (bottom bar)
// Top bar has no XTC/clock equivalents (those are bottom-only concepts), so its
// menu is just the shared prefix ending at (and including) ITEM_BATTERY.
constexpr int TOP_MENU_ITEMS = ITEM_XTC_STATUS_BAR;

const StrId menuNames[FULL_MENU_ITEMS] = {
    StrId::STR_CHAPTER_PAGE_COUNT,
    StrId::STR_BOOK_PROGRESS_PERCENTAGE,
    StrId::STR_PROGRESS_BAR,
    StrId::STR_PROGRESS_BAR_THICKNESS,
    StrId::STR_TITLE,
    StrId::STR_BATTERY,
    StrId::STR_XTC_STATUS_BAR,
    StrId::STR_CLOCK,
    StrId::STR_CLOCK_FORMAT,
    StrId::STR_CLOCK_UTC_OFFSET,
    StrId::STR_CLOCK_SYNC_NOW,
};

constexpr int CLOCK_FORMAT_ITEMS = 2;
const StrId clockFormatNames[CLOCK_FORMAT_ITEMS] = {StrId::STR_CLOCK_FORMAT_24H, StrId::STR_CLOCK_FORMAT_12H};

std::string formatUtcOffset(uint8_t biasedQ) {
  // biasedQ is in quarter-hour steps, biased by 48 (so 48 = UTC+0).
  if (biasedQ > 104) biasedQ = 48;
  int totalMinutes = (static_cast<int>(biasedQ) - 48) * 15;
  bool neg = totalMinutes < 0;
  int absMinutes = neg ? -totalMinutes : totalMinutes;
  int hours = absMinutes / 60;
  int mins = absMinutes % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "UTC%c%d:%02d", neg ? '-' : '+', hours, mins);
  return buf;
}
constexpr int PROGRESS_BAR_ITEMS = 3;
const StrId progressBarNames[PROGRESS_BAR_ITEMS] = {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE};

constexpr int PROGRESS_BAR_THICKNESS_ITEMS = 3;
const StrId progressBarThicknessNames[PROGRESS_BAR_THICKNESS_ITEMS] = {
    StrId::STR_PROGRESS_BAR_THIN, StrId::STR_PROGRESS_BAR_MEDIUM, StrId::STR_PROGRESS_BAR_THICK};

constexpr int TITLE_ITEMS = 3;
const StrId titleNames[TITLE_ITEMS] = {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE};

constexpr int XTC_STATUS_BAR_ITEMS = 3;
const StrId xtcStatusBarNames[XTC_STATUS_BAR_ITEMS] = {StrId::STR_HIDE, StrId::STR_BOTTOM, StrId::STR_TOP};

constexpr int STATUS_BAR_CLOCK_ITEMS = CrossPointSettings::STATUS_BAR_CLOCK_MODE_COUNT;
const StrId statusBarClockNames[STATUS_BAR_CLOCK_ITEMS] = {StrId::STR_HIDE, StrId::STR_DIR_RIGHT, StrId::STR_DIR_LEFT};

const int verticalPreviewPadding = 50;
const int verticalPreviewTextPadding = 40;
}  // namespace

void StatusBarSettingsActivity::onEnter() {
  Activity::onEnter();

  selectedIndex = 0;
  if (isTop()) {
    visibleItemCount = TOP_MENU_ITEMS;
  } else {
    visibleItemCount = halClock.isAvailable() ? FULL_MENU_ITEMS : BASE_MENU_ITEMS;
  }

  // Clamp progressBar/title in case of corrupt/migrated data
  uint8_t& progressBar = isTop() ? SETTINGS.topBarProgressBar : SETTINGS.statusBarProgressBar;
  if (progressBar >= PROGRESS_BAR_ITEMS) {
    progressBar = CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS;
  }

  uint8_t& progressBarThickness = isTop() ? SETTINGS.topBarProgressBarThickness : SETTINGS.statusBarProgressBarThickness;
  if (progressBarThickness >= PROGRESS_BAR_THICKNESS_ITEMS) {
    progressBarThickness = CrossPointSettings::STATUS_BAR_PROGRESS_BAR_THICKNESS::PROGRESS_BAR_NORMAL;
  }

  uint8_t& title = isTop() ? SETTINGS.topBarTitle : SETTINGS.statusBarTitle;
  if (title >= TITLE_ITEMS) {
    title = CrossPointSettings::STATUS_BAR_TITLE::HIDE_TITLE;
  }

  if (!isTop()) {
    if (SETTINGS.xtcStatusBarMode >= XTC_STATUS_BAR_ITEMS) {
      SETTINGS.xtcStatusBarMode = CrossPointSettings::XTC_STATUS_BAR_MODE::XTC_STATUS_BAR_HIDE;
    }

    if (SETTINGS.clockUtcOffsetQ > 104) {
      SETTINGS.clockUtcOffsetQ = 48;  // Default to UTC+0
    }

    if (SETTINGS.clockFormat >= CLOCK_FORMAT_ITEMS) {
      SETTINGS.clockFormat = 0;
    }

    if (SETTINGS.statusBarClock >= STATUS_BAR_CLOCK_ITEMS) {
      SETTINGS.statusBarClock = CrossPointSettings::STATUS_BAR_CLOCK_MODE::STATUS_BAR_CLOCK_HIDE;
    }
  }

  requestUpdate();
}

void StatusBarSettingsActivity::onExit() { Activity::onExit(); }

void StatusBarSettingsActivity::loop() {
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

  // Handle navigation
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

void StatusBarSettingsActivity::handleSelection() {
  switch (selectedIndex) {
    case ITEM_CHAPTER_PAGE_COUNT: {
      uint8_t& v = isTop() ? SETTINGS.topBarChapterPageCount : SETTINGS.statusBarChapterPageCount;
      v = (v + 1) % 2;
      break;
    }
    case ITEM_BOOK_PROGRESS_PERCENTAGE: {
      uint8_t& v = isTop() ? SETTINGS.topBarBookProgressPercentage : SETTINGS.statusBarBookProgressPercentage;
      v = (v + 1) % 2;
      break;
    }
    case ITEM_PROGRESS_BAR: {
      const bool top = isTop();
      uint8_t& v = top ? SETTINGS.topBarProgressBar : SETTINGS.statusBarProgressBar;
      optionPopup.show(StrId::STR_PROGRESS_BAR, progressBarNames, PROGRESS_BAR_ITEMS, v, [top](int idx) {
        (top ? SETTINGS.topBarProgressBar : SETTINGS.statusBarProgressBar) = idx;
        SETTINGS.saveToFile();
      });
      return;
    }
    case ITEM_PROGRESS_BAR_THICKNESS: {
      const bool top = isTop();
      uint8_t& v = top ? SETTINGS.topBarProgressBarThickness : SETTINGS.statusBarProgressBarThickness;
      optionPopup.show(StrId::STR_PROGRESS_BAR_THICKNESS, progressBarThicknessNames, PROGRESS_BAR_THICKNESS_ITEMS, v,
                       [top](int idx) {
                         (top ? SETTINGS.topBarProgressBarThickness : SETTINGS.statusBarProgressBarThickness) = idx;
                         SETTINGS.saveToFile();
                       });
      return;
    }
    case ITEM_TITLE: {
      const bool top = isTop();
      uint8_t& v = top ? SETTINGS.topBarTitle : SETTINGS.statusBarTitle;
      optionPopup.show(StrId::STR_TITLE, titleNames, TITLE_ITEMS, v, [top](int idx) {
        (top ? SETTINGS.topBarTitle : SETTINGS.statusBarTitle) = idx;
        SETTINGS.saveToFile();
      });
      return;
    }
    case ITEM_BATTERY: {
      uint8_t& v = isTop() ? SETTINGS.topBarBattery : SETTINGS.statusBarBattery;
      v = (v + 1) % 2;
      break;
    }
    case ITEM_XTC_STATUS_BAR:
      optionPopup.show(StrId::STR_XTC_STATUS_BAR, xtcStatusBarNames, XTC_STATUS_BAR_ITEMS, SETTINGS.xtcStatusBarMode,
                       [this](int idx) {
                         SETTINGS.xtcStatusBarMode = idx;
                         SETTINGS.saveToFile();
                       });
      return;
    case ITEM_CLOCK:
      SETTINGS.statusBarClock = (SETTINGS.statusBarClock + 1) % STATUS_BAR_CLOCK_ITEMS;
      break;
    case ITEM_CLOCK_FORMAT:
      SETTINGS.clockFormat = (SETTINGS.clockFormat + 1) % CLOCK_FORMAT_ITEMS;
      break;
    case ITEM_CLOCK_UTC_OFFSET:
      // Launch the dedicated offset picker. It saves on exit, no result handler needed.
      startActivityForResult(std::make_unique<ClockOffsetActivity>(renderer, mappedInput), nullptr);
      return;
    case ITEM_CLOCK_SYNC:
      startActivityForResult(std::make_unique<ClockSyncActivity>(renderer, mappedInput), nullptr);
      return;
    default:
      return;
  }
  SETTINGS.saveToFile();
}

void StatusBarSettingsActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();

  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight},
                I18N.get(isTop() ? StrId::STR_CUSTOMISE_TOP_STATUS_BAR : StrId::STR_CUSTOMISE_STATUS_BAR));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const bool top = isTop();
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, visibleItemCount, static_cast<int>(selectedIndex),
      [](int index) { return std::string(I18N.get(menuNames[index])); }, nullptr, nullptr,
      [top](int index) -> std::string {
        switch (index) {
          case ITEM_CHAPTER_PAGE_COUNT:
            return (top ? SETTINGS.topBarChapterPageCount : SETTINGS.statusBarChapterPageCount) ? tr(STR_SHOW)
                                                                                                 : tr(STR_HIDE);
          case ITEM_BOOK_PROGRESS_PERCENTAGE:
            return (top ? SETTINGS.topBarBookProgressPercentage : SETTINGS.statusBarBookProgressPercentage)
                       ? tr(STR_SHOW)
                       : tr(STR_HIDE);
          case ITEM_PROGRESS_BAR:
            return I18N.get(progressBarNames[top ? SETTINGS.topBarProgressBar : SETTINGS.statusBarProgressBar]);
          case ITEM_PROGRESS_BAR_THICKNESS:
            return I18N.get(progressBarThicknessNames[top ? SETTINGS.topBarProgressBarThickness
                                                            : SETTINGS.statusBarProgressBarThickness]);
          case ITEM_TITLE:
            return I18N.get(titleNames[top ? SETTINGS.topBarTitle : SETTINGS.statusBarTitle]);
          case ITEM_BATTERY:
            return (top ? SETTINGS.topBarBattery : SETTINGS.statusBarBattery) ? tr(STR_SHOW) : tr(STR_HIDE);
          case ITEM_XTC_STATUS_BAR:
            return I18N.get(xtcStatusBarNames[SETTINGS.xtcStatusBarMode]);
          case ITEM_CLOCK:
            return I18N.get(statusBarClockNames[SETTINGS.statusBarClock]);
          case ITEM_CLOCK_FORMAT: {
            const uint8_t fmt = SETTINGS.clockFormat < CLOCK_FORMAT_ITEMS ? SETTINGS.clockFormat : 0;
            return std::string(I18N.get(clockFormatNames[fmt]));
          }
          case ITEM_CLOCK_UTC_OFFSET:
            return formatUtcOffset(SETTINGS.clockUtcOffsetQ);
          case ITEM_CLOCK_SYNC:
            return SETTINGS.clockHasBeenSynced ? tr(STR_CLOCK_SYNCED) : tr(STR_NOT_SET);
          default:
            return tr(STR_HIDE);
        }
      },
      true);

  // Draw button hints
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_TOGGLE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  const uint8_t previewTitleMode = top ? SETTINGS.topBarTitle : SETTINGS.statusBarTitle;
  std::string title;
  if (previewTitleMode == CrossPointSettings::STATUS_BAR_TITLE::BOOK_TITLE) {
    title = tr(STR_EXAMPLE_BOOK);
  } else if (previewTitleMode == CrossPointSettings::STATUS_BAR_TITLE::CHAPTER_TITLE) {
    title = tr(STR_EXAMPLE_CHAPTER);
  }

  GUI.drawStatusBar(renderer, 75, 8, 32, title, verticalPreviewPadding, 0, false, false, false,
                    top ? CrossPointSettings::Edge::TOP : CrossPointSettings::Edge::BOTTOM);

  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding,
                    renderer.getScreenHeight() -
                        UITheme::getInstance().getStatusBarHeight(top ? CrossPointSettings::Edge::TOP
                                                                       : CrossPointSettings::Edge::BOTTOM) -
                        verticalPreviewPadding - verticalPreviewTextPadding,
                    tr(STR_PREVIEW));

  renderer.displayBuffer();
}
