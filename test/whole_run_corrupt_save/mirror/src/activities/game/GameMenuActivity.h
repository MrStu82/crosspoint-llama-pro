#pragma once
// Shadow of src/activities/game/GameMenuActivity.h, adapted from
// test/sponsor_hp_clamp/mirror/src/activities/game/GameMenuActivity.h.
// GameActivity.cpp's real openGameMenu()/onGameMenuResult() reference this
// type (constructor + MenuAction enum), but the whole-run-corrupt-save flow
// under test never opens the pause menu (Confirm on the notice resolves via
// resolveWholeRunCorruptNotice(), not openGameMenu()) -- so a trivial shadow
// with the real public interface shape is enough to let GameActivity.cpp
// compile and link unmodified. Matched against the real header's constructor
// signature and MenuAction enum exactly.
#include "activities/Activity.h"

class GameMenuActivity final : public Activity {
 public:
  enum class MenuAction { RESUME, SAVE_QUIT, ABANDON, THROW };

  explicit GameMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameMenu", renderer, mappedInput) {}

  void onEnter() override {}
  void loop() override {}
  void render(RenderLock&&) override {}
};
