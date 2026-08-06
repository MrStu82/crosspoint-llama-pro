#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// In-game pause menu for Deep Mines: Resume / Inventory / Character / Save & Quit / Abandon Run.
// Inventory and Character are internal sub-screens (not separate pushed activities) since they
// are simple read/act views over the GameState singleton with no independent lifecycle needs.
// Resume, Save & Quit, and Abandon Run are reported back to the caller via MenuResult so
// GameActivity can react (no std::function callback members).
class GameMenuActivity final : public Activity {
 public:
  enum class MenuAction { RESUME, SAVE_QUIT, ABANDON };

  explicit GameMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameMenu", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen { Menu, Inventory, Character };

  void useInventoryItem(int index);

  void renderMenu();
  void renderInventory();
  void renderCharacter();

  Screen currentScreen = Screen::Menu;
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;
};
