#pragma once

#include <GfxRenderer.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

// Games category hub, opened from Home. Lists the available games; selecting one launches
// it via the matching ActivityManager::goToX(). Kept deliberately flat/simple to match the
// LanguageSelectActivity list idiom -- no separate engine/base class for the games themselves.
class GamesListActivity final : public Activity {
 public:
  explicit GamesListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GamesList", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleSelection();
  void onBack() { finish(); }

  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  static constexpr int kItemCount = 2;  // Deep Mines, Tetris; Tamagotchi/Solitaire land in follow-up items
};
