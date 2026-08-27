#include "RecentBooksActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <ctime>
#include <memory>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "network/HardcoverRating.h"
#include "network/HardcoverSyncResult.h"
#include "components/UITheme.h"
#include "components/InkPointShell.h"
#include "fontIds.h"

namespace {
// Hold threshold for the long-press "remove from list" action (firmware convention).
constexpr unsigned long LONG_PRESS_MS = 1000;
constexpr unsigned long HARD_COVER_SYNC_DELAY_MS = 1250;
}  // namespace

void RecentBooksActivity::loadRecentBooks() { recentBooks = RECENT_BOOKS.getBooks(); }

void RecentBooksActivity::promptMetadataSync() {
  // The sync owns connectivity: returning from Wi-Fi is the only route that
  // starts network work, so Library never assumes a radio is already usable.
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, "Sync Hardcover metadata",
                                             "Connect to Wi-Fi and update every Library book?"),
      [this](const ActivityResult& confirmation) {
        if (confirmation.isCancelled) return;
        startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, false),
                               [this](const ActivityResult& wifi) {
          if (wifi.isCancelled) {
            syncState = SyncState::Cancelled;
            syncStatus = "Wi-Fi cancelled; nothing synced";
            requestUpdate();
            return;
          }
          beginMetadataSync();
        });
      });
}

void RecentBooksActivity::beginMetadataSync() {
  syncState = SyncState::Running;
  syncIndex = syncUpdated = syncTokenMissing = syncNetworkFailed = syncNoMatch = syncNoSuggestions = 0;
  nextSyncAtMs = 0;
  syncStatus = "Preparing Library sync";
  requestUpdate(true);
}

void RecentBooksActivity::finishCandidateChoice(bool store) {
  if (store && HardcoverSyncResult::shouldPersistSelection(candidateIndex, syncCandidates.size())) {
    if (HardcoverRating::storeLastGood(syncCandidates[candidateIndex].snapshot)) ++syncUpdated;
    else ++syncNetworkFailed;
  } else {
    ++syncNoMatch;
  }
  ++syncIndex;
  syncCandidates.clear();
  candidateIndex = 0;
  syncState = SyncState::Running;
  nextSyncAtMs = millis() + HARD_COVER_SYNC_DELAY_MS;
  requestUpdate(true);
}

void RecentBooksActivity::runOneMetadataSync() {
  if (syncState != SyncState::Running || millis() < nextSyncAtMs) return;
  if (syncIndex >= recentBooks.size()) {
    syncState = SyncState::Complete;
    syncStatus.clear();
    requestUpdate(true);
    return;
  }
  const auto& book = recentBooks[syncIndex];
  const auto now = static_cast<int64_t>(std::time(nullptr));
  const auto result = now > 0 ? HardcoverRating::refresh({book.path, book.isbn, book.title, book.author}, now)
                               : HardcoverRating::HardcoverRefreshResult{HardcoverRefreshStatus::NetworkFailure, std::nullopt};
  switch (result.status) {
    case HardcoverRefreshStatus::Updated: ++syncUpdated; break;
    case HardcoverRefreshStatus::TokenMissing: ++syncTokenMissing; break;
    case HardcoverRefreshStatus::NetworkFailure: ++syncNetworkFailed; break;
    case HardcoverRefreshStatus::NoMatch: ++syncNoMatch; ++syncNoSuggestions; break;
    case HardcoverRefreshStatus::Ambiguous:
      syncCandidates = result.candidates;
      candidateIndex = 0;
      syncState = SyncState::Choosing;
      syncStatus = "Choose Hardcover match " + std::to_string(syncIndex + 1) + "/" + std::to_string(recentBooks.size());
      requestUpdate(true);
      return;
  }
  ++syncIndex;
  syncStatus = "Syncing " + std::to_string(syncIndex) + "/" + std::to_string(recentBooks.size()) +
               "  (" + std::to_string(syncUpdated) + " updated; " +
               std::to_string(syncNoMatch) + " no match)";
  nextSyncAtMs = millis() + HARD_COVER_SYNC_DELAY_MS;
  requestUpdate();
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();

  // Prune entries whose backing files are gone; this is one of two interaction
  // points where the persistent store gets cleaned (the other is addBook).
  if (RECENT_BOOKS.pruneMissing()) {
    RECENT_BOOKS.saveToFile();
  }

  // Load data
  loadRecentBooks();

  selectorIndex = 0;
  syncState = SyncState::Idle;
  syncStatus.clear();
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::loop() {
  if (InkPointShell::enabled(renderer)) {
    if (const auto destination = InkPointShell::tappedDestination(mappedInput)) {
      InkPointShell::navigate(*destination);
      return;
    }
  }
  if (syncState == SyncState::Choosing) {
    const size_t choices = syncCandidates.size() + 1;  // final entry is Skip
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { finishCandidateChoice(candidateIndex < syncCandidates.size()); return; }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finishCandidateChoice(false); return; }
    buttonNavigator.onNextRelease([this, choices] { candidateIndex = (candidateIndex + 1) % choices; requestUpdate(); });
    buttonNavigator.onPreviousRelease([this, choices] { candidateIndex = (candidateIndex + choices - 1) % choices; requestUpdate(); });
    return;
  }
  runOneMetadataSync();

  const int pageItems = InkPointShell::enabled(renderer)
                            ? GUI.getListPageItems(InkPointShell::kFooterTop - InkPointShell::kContentTop - 8, true)
                            : UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const bool inkPoint = InkPointShell::enabled(renderer);
  const int contentTop = inkPoint ? InkPointShell::kContentTop
                                  : metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = inkPoint ? InkPointShell::kFooterTop - contentTop - 8
                                     : renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight -
                                           metrics.verticalSpacing;

  // After a long-press has fired, swallow input until Confirm is physically released
  // (so the release doesn't also open the book; re-arm only once the button is up).
  if (longPressFired) {
    if (!mappedInput.isPressed(MappedInputManager::Button::Confirm)) {
      longPressFired = false;
    }
    return;
  }

  // Long-press Confirm on the selected book: prompt to remove it from the list.
  // Fires when the hold times out while still held (firmware hold-to-act pattern,
  // cf. FileBrowserActivity BACK long-press).
  if (!isSyncCommandSelected() && selectorIndex <= recentBooks.size() &&
      mappedInput.isPressed(MappedInputManager::Button::Confirm) && mappedInput.getHeldTime() >= LONG_PRESS_MS) {
    longPressFired = true;
    promptRemoveBook(recentBooks[selectorIndex - 1].path, recentBooks[selectorIndex - 1].title);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (isSyncCommandSelected()) {
      promptMetadataSync();
      return;
    }
    if (selectorIndex <= recentBooks.size()) {
      LOG_DBG("RBA", "Selected recent book: %s", recentBooks[selectorIndex - 1].path.c_str());
      onSelectBook(recentBooks[selectorIndex - 1].path);
      return;
    }
  }

  int touchSel = static_cast<int>(selectorIndex);
  const auto listTouch =
      handleListTouch(touchSel, entryCount(), contentTop, contentHeight, true);
  if (listTouch != ListTouchResult::None) {
    selectorIndex = static_cast<size_t>(touchSel);
    if (listTouch == ListTouchResult::Activated) {
      if (isSyncCommandSelected()) promptMetadataSync();
      else {
        LOG_DBG("RBA", "Tapped recent book: %s", recentBooks[selectorIndex - 1].path.c_str());
        onSelectBook(recentBooks[selectorIndex - 1].path);
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (syncState == SyncState::Running) {
      syncState = SyncState::Cancelled;
      syncStatus = "Sync cancelled: " + std::to_string(syncIndex) + "/" +
                   std::to_string(recentBooks.size()) + " kept";
      requestUpdate(true);
    } else onGoHome();
  }

  int listSize = entryCount();
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
    return;
  }

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

void RecentBooksActivity::promptRemoveBook(const std::string& path, const std::string& title) {
  auto handler = [this, path](const ActivityResult& res) {
    if (res.isCancelled) {
      LOG_DBG("RBA", "Remove from recents cancelled");
      return;
    }
    if (RECENT_BOOKS.removeByPath(path)) {
      LOG_DBG("RBA", "Removed from recents: %s", path.c_str());
      loadRecentBooks();
      if (recentBooks.empty()) {
        selectorIndex = 0;
      } else if (selectorIndex >= recentBooks.size()) {
        selectorIndex = recentBooks.size() - 1;
      }
      requestUpdate(true);
    }
  };

  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_REMOVE_FROM_RECENTS), title),
      std::move(handler));
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const bool inkPoint = InkPointShell::enabled(renderer);
  if (inkPoint) InkPointShell::drawHeader(renderer, "Library");
  else GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

  const int contentTop = inkPoint ? InkPointShell::kContentTop
                                  : metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = inkPoint ? InkPointShell::kFooterTop - contentTop - 8
                                     : pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (syncState == SyncState::Complete) {
    const auto lines = HardcoverSyncResult::format(syncUpdated, syncNoMatch, syncNoSuggestions, syncTokenMissing, syncNetworkFailed);
    int y = contentTop + 18;
    for (const auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, metrics.contentSidePadding, y, line.c_str());
      y += 22;
    }
    renderer.displayBuffer();
    return;
  }

  if (syncState == SyncState::Choosing) {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(syncCandidates.size() + 1), candidateIndex,
      [this](int index) { return index < static_cast<int>(syncCandidates.size()) ? syncCandidates[index].title : std::string("Skip"); },
      [this](int index) { if (index >= static_cast<int>(syncCandidates.size())) return std::string("Do not cache a match"); const auto& c = syncCandidates[index]; return c.author + " (" + std::to_string(c.snapshot.publicationYear) + ")"; },
      [](int) { return UIIcon::None; });
    renderer.displayBuffer();
    return;
  }

  // The explicit command stays visible even for an empty Library.
  if (recentBooks.empty())
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, entryCount(), selectorIndex,
        [this](int index) { return index == 0 ? std::string("Sync Hardcover metadata") : recentBooks[index - 1].title; },
        [this](int index) {
          if (index != 0) return recentBooks[index - 1].author;
          return syncStatus.empty() ? std::string("Connect Wi-Fi, then update every Library book") : syncStatus;
        },
        [this](int index) { return index == 0 ? UIIcon::Wifi : UITheme::getFileIcon(recentBooks[index - 1].path); });
    if (syncState == SyncState::Running) {
      const int barY = InkPointShell::enabled(renderer) ? InkPointShell::kFooterTop - 14 : contentTop + contentHeight - 14;
      const int total = std::max(1, static_cast<int>(recentBooks.size()));
      renderer.drawRect(20, barY, pageWidth - 40, 6);
      renderer.fillRect(21, barY + 1, (pageWidth - 42) * static_cast<int>(syncIndex) / total, 4);
    }
  }

  // Help text
  if (inkPoint) {
    InkPointShell::drawFooter(renderer, InkPointShell::Destination::Library);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
