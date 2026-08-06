#pragma once

#include "activities/Activity.h"
#include "game/DungeonGenerator.h"
#include "game/GameRenderer.h"
#include "game/GameState.h"
#include "game/GameTypes.h"

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
  game::Monster monsters[game::MAX_MONSTERS];
  game::Item levelItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount = 0;
  uint8_t itemCount = 0;

  // Visibility cache (computed per turn)
  bool visible[game::MAP_SIZE];

  // Rendering
  GameRenderer gameRenderer;

  void loadOrGenerateLevel();
  void saveCurrentLevel();
  void computeVisibility();
  void handleMove(int dx, int dy);
  void handleAction();
  void processMonsterTurns();
  void monsterAttackPlayer(game::Monster& monster);
  void checkLevelUp();
  void handlePlayerDeath();
  void handleVictory();
  void openGameMenu();
  void onGameMenuResult(const ActivityResult& result);
};
