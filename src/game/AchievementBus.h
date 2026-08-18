#pragma once

#include "Achievements.h"

class AchievementBus {
  static AchievementBus instance;

  bool unlocked[static_cast<uint8_t>(game::AchievementId::Count)] = {};
  // Subset of `unlocked` earned during the CURRENT run only (Phase 7 req 2/3's
  // death/victory screen shows what this run accomplished, not the account's
  // full lifetime unlock history). Reset by resetRun(), never persisted.
  bool unlockedThisRun[static_cast<uint8_t>(game::AchievementId::Count)] = {};

  // -- Per-floor transient state, reset on FloorChanged --
  bool wasCriticalThisFloor = false;
  bool tookDamageThisFloor = false;
  bool killedAnythingThisFloor = false;
  // Turn count at the moment the CURRENT floor was entered -- per-floor timer
  // for Shortcut/Scenic Route. Distinct from the Express Descent window below.
  uint32_t turnAtCurrentFloorStart = 0;

  // -- Per-run transient state, reset by resetRun() --
  // Express Descent's rolling 3-floor window: turn count and depth captured
  // at the start of the window, floor-transition count within it.
  uint32_t turnAtFloorEntry = 0;
  uint32_t depthAtRecentFloorStart = 0;
  uint8_t floorsClearedThisWindow = 0;
  // True once the player has died in this run -- guards The Unkilled against
  // a stale LevelUp-implies-alive assumption (a LevelUp event can only ever
  // fire while the player is alive, but this flag makes that explicit rather
  // than relying on event ordering).
  bool hasDiedThisRun = false;
  // Set on any PlayerDamaged event that drops hp below 10% of maxHp; cleared
  // once Back From The Brink fires. Approximates "healed to full" via the
  // next LevelUp (which fully heals) since no dedicated heal event exists.
  bool wasCriticalThisRun = false;

  // -- Lifetime counters, persisted across runs (achievements.bin v2) --
  uint32_t lifetimeTilesWalked = 0;    // Wanderer / Pathfinder.
  uint8_t floorsExploredFully = 0;     // Cartographer / Thorough / Obsessive. Capped at 255 (Obsessive only needs 20).
  uint32_t magpiePickupCount = 0;      // Magpie: real lifetime item-pickup count, not derived/faked.

  // Queue of pending new-unlock (lifetime, not just this-run) achievement ids --
  // lets GameActivity drive a boxed System notification (Phase 9 work item 3)
  // without this class knowing anything about GameRenderer/UI. A single
  // emit() call can unlock more than one achievement at once (e.g.
  // FloorChanged's multi-check), so this is a small bounded queue rather
  // than a single flag+pointer -- otherwise a second unlock in the same
  // emit() silently overwrites the first before either is ever consumed.
  // Sized generously above the current worst case for headroom. Consumed one
  // at a time via consumeNewUnlockId() so a caller that never checks just
  // leaves the queue populated, no crash risk. Stores the id rather than a
  // pre-joined flavor string so the banner can resolve Name/Reward straight
  // from ACHIEVEMENT_DEFS -- kills the " / " concatenation that used to
  // happen at drain time.
  static constexpr uint8_t MAX_PENDING_UNLOCKS = 8;
  game::AchievementId pendingIds_[MAX_PENDING_UNLOCKS] = {};
  uint8_t pendingCount_ = 0;

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
  // Pops and returns the id of the oldest pending new unlock. Call
  // hasNewUnlock() first; returns AchievementId::Count (an out-of-range
  // sentinel, never a real id) if the queue is empty rather than crashing.
  game::AchievementId consumeNewUnlockId();

  // Called by GameActivity's move handling so lifetime tile-walk tracking
  // (Wanderer/Pathfinder) stays decoupled from the event-switch pattern used
  // for everything else -- a per-tile counter bump isn't a discrete "event".
  // Checks the Wanderer/Pathfinder thresholds itself since there is no
  // GameEvent wrapping a single footstep.
  void addTilesWalked(uint32_t n);

 private:
  void unlock(game::AchievementId id, const char* flavorText);
  bool save() const;
};

#define ACHIEVEMENTS AchievementBus::getInstance()
