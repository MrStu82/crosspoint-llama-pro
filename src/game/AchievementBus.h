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

  // Set by unlock() whenever it actually unlocks something new (lifetime,
  // not just this-run) -- lets GameActivity drive a boxed System
  // notification (Phase 9 work item 3) without this class knowing anything
  // about GameRenderer/UI. Consumed (and cleared) via consumeNewUnlockFlavor()
  // so a caller that never checks just leaves the flag set, no crash risk.
  bool hasNewUnlock_ = false;
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

  // True if the most recent emit() call unlocked something new (lifetime),
  // not yet consumed. GameActivity polls this after each emit() call.
  bool hasNewUnlock() const { return hasNewUnlock_; }
  // Returns the flavor text for the pending new unlock (flash-resident
  // literal passed to unlock(), copied into a fixed buffer -- no heap) and
  // clears hasNewUnlock().
  const char* consumeNewUnlockFlavor();

 private:
  void unlock(game::AchievementId id, const char* flavorText);
  bool save() const;
};

#define ACHIEVEMENTS AchievementBus::getInstance()
