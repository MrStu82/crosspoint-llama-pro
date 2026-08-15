#pragma once
// Shadow of src/activities/game/GameActivity.h. GameTitleActivity::render() (the harness's
// actual runtime path) never reaches GameActivity at all -- only GameTitleActivity::loop()
// does (via std::make_unique<GameActivity> on button press, never called by the harness).
// But GameTitleActivity.cpp is one translation unit compiled unmodified, so that
// make_unique<GameActivity> call must still compile+link even though it's runtime-dead --
// and the real header's private gameplay state (game::Tile[], GameRenderer, etc.) drags in
// the entire dungeon/combat/rendering subsystem just to satisfy that. Giving every override
// a trivial INLINE body here (rather than leaving them declared-only, as the real header
// does) keeps the vtable's key function local to this header, so no GameActivity.cpp/
// GameRenderer.cpp/DungeonGenerator.cpp linkage is required at all. Doesn't touch
// GameTitleActivity's own real draw calls or fonts in any way.
#include "activities/Activity.h"

class GameActivity final : public Activity {
 public:
  explicit GameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Game", renderer, mappedInput) {}

  void onEnter() override {}
  void loop() override {}
  void render(RenderLock&&) override {}
  bool preventAutoSleep() override { return true; }
  bool handleHomeGesture() override { return false; }
};
