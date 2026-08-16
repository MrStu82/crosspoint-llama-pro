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
  enum class MenuAction { RESUME, SAVE_QUIT, ABANDON, THROW };

  explicit GameMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameMenu", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen { Menu, Inventory, Character, Achievements, ThrowTarget };

  void useInventoryItem(int index);

  void renderMenu();
  void renderInventory();
  void renderCharacter();
  void renderAchievements();
  void renderThrowTarget();

  // Touch support for the Screen::Menu row list (Resume/Inventory/Character/Save &
  // Quit/Abandon Run), additive alongside the existing physical-button navigation.
  // Single-tap-to-activate: a tap on a row both selects it and immediately fires the
  // same action the Confirm button would. Row geometry mirrors BaseTheme::drawButtonMenu()
  // exactly (see GameMenuActivity.cpp) so hit-test rects always match what's drawn.
  // Returns true if the tap was consumed (landed on a menu row).
  bool handleMenuTouch();

  // Touch support for the Screen::Inventory row list, additive alongside the existing
  // physical-button navigation. Uses the shared Activity::handleListTouch() helper against
  // the same content band renderInventory()'s GUI.drawList() draws into, so hit-test rows
  // always match what's drawn. Returns true if the touch was consumed (landed on a row).
  bool handleInventoryTouch();

  Screen currentScreen = Screen::Menu;
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;

  // Screen::Inventory's Confirm button is overloaded: a short press uses/equips the
  // hovered item, a long press on a throwable item instead enters Screen::ThrowTarget.
  // longPressFired latches once the hold threshold trips so the eventual release doesn't
  // also fire the short-press action (same pattern as RecentBooksActivity.cpp).
  bool longPressFired = false;

  // Inventory index of the item being thrown, captured when entering Screen::ThrowTarget
  // so the later direction press can report it back to GameActivity via MenuResult.
  int throwItemIndex = -1;
};
