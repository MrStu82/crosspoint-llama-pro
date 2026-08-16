#pragma once

#include "activities/Activity.h"
#include "game/DungeonGenerator.h"
#include "game/GameRenderer.h"
#include "game/GameState.h"
#include "game/GameTypes.h"

// Which screen GameActivity is currently presenting. Playing is the normal loop/render
// path; Death/Victory are the blocking end-of-run screens (Phase 7 req 2/3) — deliberately
// thin (GameRenderer::drawEndScreen() never calls clearScreen()) since Phase 8 rewrites the
// redraw model underneath them. CorruptSaveNotice is the blocking two-option System notice
// shown when loadOrGenerateLevel() rejects a level save (Phase 12 corrupt-save handling).
enum class GameScreenMode : uint8_t { Playing, Death, Victory, CorruptSaveNotice };

// Which save CorruptSaveNotice is currently reporting on -- see corruptNoticeScope below.
enum class CorruptNoticeScope : uint8_t { PerLevel, WholeRun };

// Main Deep Mines gameplay loop: dungeon viewport, movement, combat, item pickup, and
// stairs/level transitions. The in-game pause menu (GameMenuActivity) is launched via
// startActivityForResult and reports back which action was taken via MenuResult.
class GameActivity final : public Activity {
 public:
  explicit GameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Game", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
  // A mid-run home-key long-hold/swipe would otherwise fall through to
  // ActivityManager's default goHome(), silently abandoning the current run
  // with no save prompt. Route it into the existing pause menu instead, same
  // as EpubReaderMenuActivity does for its own modal.
  bool handleHomeGesture() override;

 private:
  // Level data (~5.5KB total)
  game::Tile tiles[game::MAP_SIZE];
  uint8_t fogOfWar[game::FOG_SIZE];
  // Bitmap of door tiles the player has opened this level, persisted across save/reload
  // (Phase 7 req 6) so a reload doesn't revert opened doors back to DoorClosed.
  uint8_t doorOpen[game::FOG_SIZE];
  game::Monster monsters[game::MAX_MONSTERS];
  game::Item levelItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount = 0;
  uint8_t itemCount = 0;

  // Visibility cache (computed per turn)
  bool visible[game::MAP_SIZE];

  // Rendering
  GameRenderer gameRenderer;

  // End-of-run screen state (Phase 7 req 2/3). deathCause is populated by
  // monsterAttackPlayer() right before handlePlayerDeath() is invoked.
  GameScreenMode screenMode = GameScreenMode::Playing;
  char deathCause[32] = "";
  // Snapshot of the stats/achievements shown on the end screen, taken at the
  // moment of death/victory -- render() has no other way to reach this data
  // from RenderLock&&, and the underlying GameState/AchievementBus values keep
  // changing shape once a new run starts (which onGoHome()'s eventual dismiss
  // can trigger before the overlay is torn down).
  EndScreenData endScreenData;

  // CorruptSaveNotice state (Phase 12, extended to whole-run rejection): which save was
  // rejected -- PerLevel (a single level_NN.bin, set by loadOrGenerateLevel()) or WholeRun
  // (save.bin itself, set by onEnter() when GameState::loadFromFile() returns false) --
  // the floor number to report in the notice body (PerLevel only), and which option is
  // currently highlighted. Purge is index 0 (default highlight per spec), Leave is index 1.
  // Only meaningful while screenMode == CorruptSaveNotice. The two scopes resolve Confirm
  // differently (see loop()): PerLevel's freshly generated floor is already authoritative
  // either way, but WholeRun has no valid run state at all until newGame() runs, so both of
  // its options must call newGame() before play begins.
  CorruptNoticeScope corruptNoticeScope = CorruptNoticeScope::PerLevel;
  uint8_t corruptNoticeDepth = 0;
  uint8_t corruptNoticeSelection = 0;

  void loadOrGenerateLevel();
  void saveCurrentLevel();
  // Resolves a WholeRun CorruptSaveNotice: purge=true deletes save.bin first, purge=false
  // leaves it on disk untouched -- either way calls GAME_STATE.newGame() so play never
  // begins on the state a rejected loadFromFile() left behind, then proceeds into a fresh
  // run exactly as GameTitleActivity's own "start new game" path does.
  void resolveWholeRunCorruptNotice(bool purge);
  void computeVisibility();
  void handleMove(int dx, int dy);
  void handleAction();
  // Single choke point for draining ACHIEVEMENTS' pending-unlock queue and
  // surfacing it as exactly one boxed notification. A single game event can
  // unlock more than one achievement at once (e.g. FloorChanged's triple
  // check, or a thrown boss-overkill unlocking both PercussiveMaintenance
  // and EscalationOfForce) -- showNotification() only has room for one body
  // at a time, so multiple pending flavors are joined into one message
  // rather than the second silently overwriting the first before either
  // renders. No-op if nothing is pending.
  void showPendingAchievementNotifications();
  // Resolves a throw committed from GameMenuActivity's Screen::ThrowTarget: consumes
  // inventoryIndex's item, finds the nearest monster in line along dir, applies
  // dexterity-based damage, and emits GameEventType::ItemThrown.
  void handleThrow(game::Direction dir, int inventoryIndex);
  // Returns true if the player died during this batch of monster turns (caller
  // should stop processing further turns/input once that happens).
  bool processMonsterTurns();
  void monsterAttackPlayer(game::Monster& monster);
  void checkLevelUp();
  void handlePlayerDeath();
  void handleVictory();
  // Snapshots GAME_STATE.player + ACHIEVEMENTS.isUnlockedThisRun() into
  // endScreenData. Called once, right before screenMode flips -- run state
  // (e.g. a new run's fresh turnCount/kills) can't be trusted to still match
  // by the time render() actually paints the overlay.
  void populateEndScreenData();
  void openGameMenu();
  void onGameMenuResult(const ActivityResult& result);
};
