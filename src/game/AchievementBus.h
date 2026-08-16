#pragma once

#include "Achievements.h"

class AchievementBus {
  static AchievementBus instance;

  bool unlocked[static_cast<uint8_t>(game::AchievementId::Count)] = {};
  // Subset of `unlocked` earned during the CURRENT run only (Phase 7 req 2/3's
  // death/victory screen shows what this run accomplished, not the account's
  // full lifetime unlock history). Reset by resetRun(), never persisted.
  bool unlockedThisRun[static_cast<uint8_t>(game::AchievementId::Count)] = {};
  bool wasCriticalThisFloor = false;

  // Queue of pending new-unlock (lifetime, not just this-run) flavor texts --
  // lets GameActivity drive a boxed System notification (Phase 9 work item 3)
  // without this class knowing anything about GameRenderer/UI. A single
  // emit() call can unlock more than one achievement at once (e.g.
  // FloorChanged's triple check), so this is a small bounded queue rather
  // than a single flag+pointer -- otherwise a second unlock in the same
  // emit() silently overwrites the first before either is ever consumed.
  // Sized to the current worst case (FloorChanged: 3). Consumed one at a
  // time via consumeNewUnlockFlavor() so a caller that never checks just
  // leaves the queue populated, no crash risk.
  static constexpr uint8_t MAX_PENDING_UNLOCKS = 3;
  char pendingFlavors_[MAX_PENDING_UNLOCKS][96] = {};
  uint8_t pendingCount_ = 0;
  // Scratch return buffer for consumeNewUnlockFlavor() -- callers get a
  // stable const char* back, not a reference into the queue storage that
  // shifts on pop.
  char lastUnlockFlavor_[96] = "";

 public:
  static AchievementBus& getInstance() { return instance; }

  // Load unlock state from achievements.bin. Absent file == nothing unlocked.
  void load();

  // Clears the run-scoped unlock set. Call once per new run (GameState::newGame()),
  // not on save-reload of an in-progress run.
  void resetRun();

  // Game logic calls this and knows nothing about achievement identities.
  void emit(const game::GameEvent& event);

  bool isUnlocked(game::AchievementId id) const;
  bool isUnlockedThisRun(game::AchievementId id) const;

  // True if there is at least one pending new unlock (lifetime), not yet
  // consumed. GameActivity polls this in a loop (via
  // showPendingAchievementNotifications()) to drain everything queued by
  // the most recent emit() call.
  bool hasNewUnlock() const { return pendingCount_ > 0; }
  // Pops and returns the flavor text for the oldest pending new unlock
  // (flash-resident literal passed to unlock(), copied into a fixed buffer
  // -- no heap). Call hasNewUnlock() first; returns "" if the queue is
  // empty rather than crashing.
  const char* consumeNewUnlockFlavor();

 private:
  void unlock(game::AchievementId id, const char* flavorText);
  bool save() const;
};

#define ACHIEVEMENTS AchievementBus::getInstance()
