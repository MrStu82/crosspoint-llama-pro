#pragma once
// Shadow of src/activities/game/GameMenuActivity.h for the sponsor HP-clamp regression
// harness. GameActivity.cpp's real openGameMenu()/onGameMenuResult() paths construct and
// dispatch to a GameMenuActivity, but this harness only exercises loadOrGenerateLevel()
// (never opens the pause menu), so a trivial shadow with the real public interface
// (constructor signature + MenuAction enum, matched exactly against the real header) is
// enough to let GameActivity.cpp compile and link unmodified -- same "shadow header,
// real .cpp never actually reaches it" technique test/game_title_render/ already uses for
// GameActivity itself. See test/game_title_render/README's "Why mirror/ exists" section.
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
