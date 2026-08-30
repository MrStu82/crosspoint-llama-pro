#include "ReaderToolsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "ReaderToolsGeometry.h"
#include "fontIds.h"

ReaderToolsActivity::ReaderToolsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const Format format)
    : Activity("ReaderTools", renderer, mappedInput) {
  auto add = [this](const Action action, const StrId label) { items[itemCount++] = {action, label}; };
  add(Action::GoToPercent, StrId::STR_GO_TO_PERCENT);
  if (format == Format::Epub) {
    add(Action::AddBookmark, StrId::STR_ADD_BOOKMARK);
    add(Action::Bookmarks, StrId::STR_BOOKMARKS);
    add(Action::Dictionary, StrId::STR_DICTIONARY);
    add(Action::KOReaderSync, StrId::STR_READER_TOOLS_KOREADER_SYNC);
  }
  if (format != Format::Xtc) {
    add(Action::TextSettings, StrId::STR_READER_TOOLS_TEXT_SETTINGS);
  }
}

void ReaderToolsActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void ReaderToolsActivity::closeCancelled() {
  ActivityResult cancelled;
  cancelled.isCancelled = true;
  setResult(std::move(cancelled));
  finish();
}

bool ReaderToolsActivity::handleHomeGesture() {
  closeCancelled();
  return true;
}

void ReaderToolsActivity::activate(const int index) {
  if (index < 0 || index >= itemCount) return;
  setResult(MenuResult{static_cast<int>(items[index].action)});
  finish();
}

void ReaderToolsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closeCancelled();
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) {
    const auto panel = ReaderToolsGeometry::layout(renderer.getScreenWidth(), renderer.getScreenHeight(), itemCount);
    const int row = ReaderToolsGeometry::hitRow(panel, x, y, itemCount);
    if (row >= 0) {
      activate(row);
    } else if (!ReaderToolsGeometry::contains(panel, x, y)) {
      closeCancelled();
    }
    return;
  }

  buttonNavigator.onPrevious([this] {
    selectedIndex = selectedIndex < 0 ? itemCount - 1 : ButtonNavigator::previousIndex(selectedIndex, itemCount);
    requestUpdate();
  });
  buttonNavigator.onNext([this] {
    selectedIndex = selectedIndex < 0 ? 0 : ButtonNavigator::nextIndex(selectedIndex, itemCount);
    requestUpdate();
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activate(selectedIndex < 0 ? 0 : selectedIndex);
  }
}

void ReaderToolsActivity::render(RenderLock&&) {
  const auto panel = ReaderToolsGeometry::layout(renderer.getScreenWidth(), renderer.getScreenHeight(), itemCount);
  const int shadowX = panel.x + ReaderToolsGeometry::SHADOW_OFFSET;
  const int shadowY = panel.y + ReaderToolsGeometry::SHADOW_OFFSET;
  renderer.fillRoundedRect(shadowX, shadowY, panel.width, panel.height, ReaderToolsGeometry::CORNER_RADIUS,
                           Color::Black);
  renderer.fillRoundedRect(panel.x, panel.y, panel.width, panel.height, ReaderToolsGeometry::CORNER_RADIUS,
                           Color::White);
  renderer.drawRoundedRect(panel.x, panel.y, panel.width, panel.height, ReaderToolsGeometry::BORDER_WIDTH,
                           ReaderToolsGeometry::CORNER_RADIUS, true);

  const int titleTop = panel.y + (ReaderToolsGeometry::HEADER_HEIGHT - renderer.getTextHeight(NOTOSANS_18_FONT_ID)) / 2;
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, titleTop, I18N.get(StrId::STR_READER_TOOLS), true,
                            EpdFontFamily::BOLD);
  renderer.drawLine(panel.x, panel.y + ReaderToolsGeometry::HEADER_HEIGHT,
                    panel.x + panel.width - 1, panel.y + ReaderToolsGeometry::HEADER_HEIGHT, 3, true);

  for (int i = 0; i < itemCount; i++) {
    const int rowTop = panel.y + ReaderToolsGeometry::HEADER_HEIGHT + i * panel.rowHeight;
    const bool selected = i == selectedIndex;
    if (selected) renderer.fillRect(panel.x + ReaderToolsGeometry::BORDER_WIDTH, rowTop + 2,
                                    panel.width - ReaderToolsGeometry::BORDER_WIDTH * 2,
                                    panel.rowHeight - 2, true);
    const int textTop = rowTop + (panel.rowHeight - renderer.getTextHeight(NOTOSANS_16_FONT_ID)) / 2;
    renderer.drawText(NOTOSANS_16_FONT_ID, panel.x + 24, textTop, I18N.get(items[i].label), !selected,
                      EpdFontFamily::BOLD);
    if (i + 1 < itemCount) {
      renderer.drawLine(panel.x, rowTop + panel.rowHeight, panel.x + panel.width - 1,
                        rowTop + panel.rowHeight, 2, true);
    }
  }
  renderer.displayWindow(panel.x, panel.y, panel.width + ReaderToolsGeometry::SHADOW_OFFSET,
                         panel.height + ReaderToolsGeometry::SHADOW_OFFSET);
}
