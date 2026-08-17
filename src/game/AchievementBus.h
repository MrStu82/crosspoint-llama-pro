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

  // ---- Per-floor transient state (Phase 12, reset every FloorChanged and on
  // resetRun() -- never persisted, this-run/this-floor only) ----
  bool tookDamageThisFloor = false;   // Untouched
  bool killedAnythingThisFloor = false;  // Pacifist Run
  bool exploredBeforeKillThisFloor = true;  // Cornered -- set false the moment a kill
                                             // happens before the floor's fully explored
  uint32_t turnAtFloorEntry = 0;      // Shortcut / Scenic Route
  uint32_t depthAtRecentFloorStart = 0;  // Express Descent's 3-floors-in-200-turns window
  uint8_t floorsClearedThisWindow = 0;

  // Double Tap: turn number of the most recent kill vs the current turn on
  // the next kill.
  uint32_t lastKillTurn = 0xFFFFFFFFu;

  // Waste Not: turn a single most-recently-picked-up item entered the
  // inventory, and whether it's still sitting there unused. Only needs to
  // track the single latest pickup -- "used on the same turn you picked it
  // up" can, by definition, only ever refer to the most recent one.
  uint32_t lastPickupTurn = 0xFFFFFFFFu;

  // ---- Cross-run lifetime counters (Phase 12, persisted in achievements.bin
  // v2 -- explicit version-gated fields, save.bin/Player untouched) ----
  uint32_t lifetimeTilesWalked = 0;      // Wanderer / Pathfinder
  uint16_t highestWeaponAttackSeen = 0;  // Blademaster -- max ItemDef::attack ever seen
                                         // on an equipped weapon, across all runs
  uint8_t floorsExploredFully = 0;       // Thorough / Obsessive (lifetime count, not per-run)
  uint8_t lifetimeLoreUnlocks = 0;       // Archivist -- count of AchievementReward::LoreUnlock
                                         // achievements earned so far (self-referential by
                                         // design: an unlock() that grants LoreUnlock bumps
                                         // this counter, checked on the NEXT emit())
  uint8_t sameSponsorStreak = 0;         // Brand Loyalty
  uint8_t lastSponsorSeen = 0xFFu;       // 0xFF = "no floor seen yet this account"

  // Queue of pending new-unlock (lifetime, not just this-run) flavor texts --
  // lets GameActivity drive a boxed System notification (Phase 9 work item 3)
  // without this class knowing anything about GameRenderer/UI. A single
  // emit() call can unlock more than one achievement at once (e.g.
  // FloorChanged's multi-check), so this is a small bounded queue rather
  // than a single flag+pointer -- otherwise a second unlock in the same
  // emit() silently overwrites the first before either is ever consumed.
  // Bumped 3->8 for Phase 12 (FloorChanged alone can now fire up to 6-7
  // checks in the worst case). Consumed one at a time via
  // consumeNewUnlockFlavor() so a caller that never checks just leaves the
  // queue populated, no crash risk.
  static constexpr uint8_t MAX_PENDING_UNLOCKS = 8;
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

  // Clears the run-scoped unlock set and per-floor transient state. Call once
  // per new run (GameState::newGame()), not on save-reload of an in-progress
  // run.
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

  // Lifetime tile-walk counter (Wanderer/Pathfinder) -- incremented by
  // GameActivity on every successful player move, one call site, since it's
  // a plain counter with no other trigger semantics worth routing through
  // the GameEvent bus.
  void addTilesWalked(uint32_t n) { lifetimeTilesWalked += n; }

 private:
  void unlock(game::AchievementId id, const char* flavorText);
  bool save() const;
};

#define ACHIEVEMENTS AchievementBus::getInstance()
