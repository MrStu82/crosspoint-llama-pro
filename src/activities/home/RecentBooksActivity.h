#pragma once
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "network/HardcoverRating.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class RecentBooksActivity final : public Activity {
 private:
  ButtonNavigator buttonNavigator;

  size_t selectorIndex = 0;

  // Set when a long-press has fired; input is swallowed until Confirm is released
  // again so the release doesn't also open the book.
  bool longPressFired = false;

  // Recent tab state
  std::vector<RecentBook> recentBooks;

  enum class SyncState { Idle, Running, Choosing, Complete, Cancelled };
  SyncState syncState = SyncState::Idle;
  size_t syncIndex = 0;
  size_t syncUpdated = 0;
  size_t syncTokenMissing = 0;
  size_t syncNetworkFailed = 0;
  size_t syncNoMatch = 0;
  size_t syncNoSuggestions = 0;
  unsigned long nextSyncAtMs = 0;
  std::string syncStatus;
  std::vector<HardcoverCandidate> syncCandidates;
  size_t candidateIndex = 0;
  void finishCandidateChoice(bool store);

  // Data loading
  void loadRecentBooks();
  void promptMetadataSync();
  void beginMetadataSync();
  void runOneMetadataSync();
  int entryCount() const { return static_cast<int>(recentBooks.size()) + 1; };
  bool isSyncCommandSelected() const { return selectorIndex == 0; }

  // Show an OK/Cancel prompt to remove the given book from the Recent Books list.
  void promptRemoveBook(const std::string& path, const std::string& title);

 public:
  explicit RecentBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RecentBooks", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
