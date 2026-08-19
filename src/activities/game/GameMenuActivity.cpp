#include "GameMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "game/AchievementBus.h"
#include "game/GameState.h"
#include "game/GameTheme.h"
#include "game/GameTypes.h"
#include "game/HungerClock.h"
#include "game/Pet.h"
#include "activities/ActivityResult.h"

namespace {
// Hold threshold for the long-press "throw item" action on Screen::Inventory (firmware
// convention, matches RecentBooksActivity.cpp's long-press-to-remove threshold).
constexpr unsigned long LONG_PRESS_MS = 1000;

// Looks up the ItemDef backing a live inventory Item by (type, subtype), the same match
// renderInventory()'s name/category lookups already use inline. Returns nullptr if the
// item's type/subtype somehow doesn't match any table entry (should not happen in practice).
const game::ItemDef* findItemDef(const game::Item& item) {
  for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
    if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
      return &game::ITEM_DEFS[d];
    }
  }
  return nullptr;
}
}  // namespace

// --- Lifecycle ---

void GameMenuActivity::onEnter() {
  Activity::onEnter();
  currentScreen = Screen::Menu;
  selectedIndex = 0;
  requestUpdate();
}

// --- Input ---

void GameMenuActivity::loop() {
  using Button = MappedInputManager::Button;

  switch (currentScreen) {
    case Screen::Menu: {
      constexpr int menuSize = 8;  // Resume, Inventory, Character, Pet, Achievements, Theme, Save & Quit, Abandon

      buttonNavigator.onNextRelease([this] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, menuSize);
        requestUpdate();
      });

      buttonNavigator.onPreviousRelease([this] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, menuSize);
        requestUpdate();
      });

      if (mappedInput.wasReleased(Button::Confirm)) {
        switch (selectedIndex) {
          case 0:  // Resume
            setResult(MenuResult{static_cast<int>(MenuAction::RESUME), 0, 0});
            finish();
            return;
          case 1:  // Inventory
            currentScreen = Screen::Inventory;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 2:  // Character
            currentScreen = Screen::Character;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 3:  // Pet
            currentScreen = Screen::Pet;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 4:  // Achievements
            currentScreen = Screen::Achievements;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 5:  // Theme
            SETTINGS.gameTheme = (SETTINGS.gameTheme + 1) % CrossPointSettings::GAME_THEME_COUNT;
            SETTINGS.saveToFile();
            requestUpdate();
            break;
          case 6:  // Save & Quit
            setResult(MenuResult{static_cast<int>(MenuAction::SAVE_QUIT), 0, 0});
            finish();
            return;
          case 7:  // Abandon Run
            setResult(MenuResult{static_cast<int>(MenuAction::ABANDON), 0, 0});
            finish();
            return;
        }
      }

      if (mappedInput.wasReleased(Button::Back)) {
        setResult(MenuResult{static_cast<int>(MenuAction::RESUME), 0, 0});
        finish();
        return;
      }

      // Touch: additive alternate input path alongside the physical-button handling
      // above. A tap landing on a row selects it and immediately activates it (see
      // handleMenuTouch()). If activation finished the activity, return immediately
      // exactly like the physical-button paths above do.
      if (handleMenuTouch()) {
        return;
      }
      break;
    }

    case Screen::Inventory: {
      int invCount = static_cast<int>(GAME_STATE.inventoryCount);
      if (invCount == 0) {
        // Empty inventory — just go back
        if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
          currentScreen = Screen::Menu;
          selectedIndex = 1;
          requestUpdate();
        }
        break;
      }

      buttonNavigator.onNextRelease([this, invCount] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, invCount);
        requestUpdate();
      });

      buttonNavigator.onPreviousRelease([this, invCount] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, invCount);
        requestUpdate();
      });

      // After a long-press has fired, swallow input until Confirm is physically released
      // (so the release doesn't also use/equip the item; same idiom as RecentBooksActivity.cpp).
      if (longPressFired) {
        if (!mappedInput.isPressed(Button::Confirm)) {
          longPressFired = false;
        }
        break;
      }

      // Long-press Confirm on a throwable item: enter Screen::ThrowTarget instead of
      // using/equipping it. Non-throwable items (armor, scrolls, food, etc.) ignore the
      // hold entirely and fall through to the ordinary short-press handling below.
      {
        const auto& hovered = GAME_STATE.inventory[selectedIndex];
        const auto* def = findItemDef(hovered);
        if (def != nullptr && def->throwable && mappedInput.isPressed(Button::Confirm) &&
            mappedInput.getHeldTime() >= LONG_PRESS_MS) {
          longPressFired = true;
          throwItemIndex = selectedIndex;
          currentScreen = Screen::ThrowTarget;
          requestUpdate();
          break;
        }
      }

      if (mappedInput.wasReleased(Button::Confirm)) {
        useInventoryItem(selectedIndex);
        requestUpdate();
      }

      if (mappedInput.wasReleased(Button::Back)) {
        currentScreen = Screen::Menu;
        selectedIndex = 1;
        requestUpdate();
      }

      // Touch: same dual-phase idiom as handleMenuTouch() -- touch-down previews the
      // highlighted row, a tap selects and immediately activates it (mirrors Confirm).
      if (handleInventoryTouch()) {
        return;
      }
      break;
    }

    case Screen::ThrowTarget: {
      // Any of the four directions commits the throw; Back cancels without spending
      // a turn. Reported back via MenuResult (orientation = Direction, pageTurnOption =
      // inventory index) so GameActivity can resolve the throw itself -- this activity
      // owns UI only, not throw resolution/combat math.
      if (mappedInput.wasReleased(Button::Back)) {
        currentScreen = Screen::Inventory;
        throwItemIndex = -1;
        requestUpdate();
        break;
      }

      auto commitThrow = [this](game::Direction dir) {
        setResult(MenuResult{static_cast<int>(MenuAction::THROW), static_cast<uint8_t>(dir),
                             static_cast<uint8_t>(throwItemIndex)});
        finish();
      };

      if (mappedInput.wasReleased(Button::Up)) {
        commitThrow(game::Direction::North);
        return;
      }
      if (mappedInput.wasReleased(Button::Down)) {
        commitThrow(game::Direction::South);
        return;
      }
      if (mappedInput.wasReleased(Button::Left)) {
        commitThrow(game::Direction::West);
        return;
      }
      if (mappedInput.wasReleased(Button::Right)) {
        commitThrow(game::Direction::East);
        return;
      }
      break;
    }

    case Screen::Character: {
      if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
        currentScreen = Screen::Menu;
        selectedIndex = 2;
        requestUpdate();
      }

      // Touch: Character has no selectable rows, so any tap on the screen just backs out,
      // same as physical Back/Confirm above.
      int tx, ty;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        currentScreen = Screen::Menu;
        selectedIndex = 2;
        requestUpdate();
        return;
      }
      break;
    }

    case Screen::Pet: {
      if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
        currentScreen = Screen::Menu;
        selectedIndex = 3;
        requestUpdate();
      }

      // Touch: Pet has no selectable rows, same back-out-on-any-tap idiom as
      // Screen::Character.
      int tx, ty;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        currentScreen = Screen::Menu;
        selectedIndex = 3;
        requestUpdate();
        return;
      }
      break;
    }

    case Screen::Achievements: {
      if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
        currentScreen = Screen::Menu;
        selectedIndex = 4;
        requestUpdate();
        break;
      }

      // Touch: view-only screen, same back-out-on-any-tap idiom as Screen::Character.
      int tx, ty;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        currentScreen = Screen::Menu;
        selectedIndex = 4;
        requestUpdate();
        return;
      }

      int unlockedCount = 0;
      for (uint8_t i = 0; i < static_cast<uint8_t>(game::AchievementId::Count); i++) {
        if (ACHIEVEMENTS.isUnlocked(static_cast<game::AchievementId>(i))) {
          unlockedCount++;
        }
      }
      if (unlockedCount == 0) {
        break;
      }

      buttonNavigator.onNextRelease([this, unlockedCount] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, unlockedCount);
        requestUpdate();
      });

      buttonNavigator.onPreviousRelease([this, unlockedCount] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, unlockedCount);
        requestUpdate();
      });

      break;
    }
  }
}

// --- Touch (Screen::Menu) ---

bool GameMenuActivity::handleMenuTouch() {
  constexpr int menuSize = 8;  // Resume, Inventory, Character, Pet, Achievements, Theme, Save & Quit, Abandon

  // Row geometry must mirror BaseTheme::drawButtonMenu() exactly (see renderMenu()
  // for how contentTop/rect are computed, and BaseTheme.cpp's drawButtonMenu() for
  // the per-row tileY formula) — not guessed independently, so hit-test rects always
  // match what's actually drawn.
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const Rect rect(0, contentTop, pageWidth, contentHeight);

  const int rowX = rect.x + metrics.contentSidePadding;
  const int rowWidth = rect.width - metrics.contentSidePadding * 2;

  auto rowContains = [&](int index, int x, int y) {
    const int tileY = metrics.verticalSpacing + rect.y + index * (metrics.menuRowHeight + metrics.menuSpacing);
    return x >= rowX && x < rowX + rowWidth && y >= tileY && y < tileY + metrics.menuRowHeight;
  };

  int tx = 0;
  int ty = 0;

  if (mappedInput.wasScreenTouchDown(tx, ty)) {
    for (int i = 0; i < menuSize; i++) {
      if (rowContains(i, tx, ty)) {
        if (selectedIndex != i) {
          selectedIndex = i;
          requestUpdate();
        }
        return true;
      }
    }
    return false;
  }

  if (mappedInput.wasScreenTapped(tx, ty)) {
    for (int i = 0; i < menuSize; i++) {
      if (!rowContains(i, tx, ty)) continue;

      // Single-tap-to-activate: select the row and immediately fire the same
      // action the physical Confirm button would (not a two-step select-then-confirm).
      selectedIndex = i;
      switch (i) {
        case 0:  // Resume
          setResult(MenuResult{static_cast<int>(MenuAction::RESUME), 0, 0});
          finish();
          return true;
        case 1:  // Inventory
          currentScreen = Screen::Inventory;
          selectedIndex = 0;
          requestUpdate();
          return true;
        case 2:  // Character
          currentScreen = Screen::Character;
          selectedIndex = 0;
          requestUpdate();
          return true;
        case 3:  // Pet
          currentScreen = Screen::Pet;
          selectedIndex = 0;
          requestUpdate();
          return true;
        case 4:  // Achievements
          currentScreen = Screen::Achievements;
          selectedIndex = 0;
          requestUpdate();
          return true;
        case 5:  // Theme
          SETTINGS.gameTheme = (SETTINGS.gameTheme + 1) % CrossPointSettings::GAME_THEME_COUNT;
          SETTINGS.saveToFile();
          requestUpdate();
          return true;
        case 6:  // Save & Quit
          setResult(MenuResult{static_cast<int>(MenuAction::SAVE_QUIT), 0, 0});
          finish();
          return true;
        case 7:  // Abandon Run
          setResult(MenuResult{static_cast<int>(MenuAction::ABANDON), 0, 0});
          finish();
          return true;
      }
    }
    return false;
  }

  return false;
}

// --- Touch (Screen::Inventory) ---

bool GameMenuActivity::handleInventoryTouch() {
  const int invCount = static_cast<int>(GAME_STATE.inventoryCount);
  if (invCount == 0) return false;

  // Content band must mirror renderInventory()'s GUI.drawList() rect exactly, so hit-test
  // rows always match what's actually drawn (same requirement as handleMenuTouch()'s
  // BaseTheme::drawButtonMenu() mirror above).
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  auto result = handleListTouch(selectedIndex, invCount, contentTop, contentHeight, /*hasSubtitle=*/true);
  switch (result) {
    case ListTouchResult::Consumed:
      return true;
    case ListTouchResult::Activated:
      useInventoryItem(selectedIndex);
      requestUpdate();
      return true;
    case ListTouchResult::None:
      return false;
  }
  return false;
}

// --- Use Inventory Item ---

void GameMenuActivity::useInventoryItem(int index) {
  if (index < 0 || index >= GAME_STATE.inventoryCount) return;

  auto& item = GAME_STATE.inventory[index];
  auto& p = GAME_STATE.player;
  auto type = static_cast<game::ItemType>(item.type);

  bool consumed = false;
  // 160, not 128 -- the loot box gold-win message with a sponsor courtesy clause appended
  // (Phase 11) is the longest message in this switch: fixed text (112) + longest gold roll
  // "999" (3) + longest sponsor name "The Adjudicator's Legal Team" / "System Uptime
  // Guarantee (tm)" (28) + null = 144, rounded up with headroom so a future sponsor/item
  // name addition doesn't silently truncate mid-punchline.
  char msgBuf[160];

  switch (type) {
    case game::ItemType::Potion:
      if (item.subtype == 0) {  // Healing
        uint16_t heal = game::effectiveMaxHp(p) / 3;
        if (heal < 5) heal = 5;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), game::effectiveMaxHp(p));
        snprintf(msgBuf, sizeof(msgBuf), "You feel better! (HP +%u)", heal);
        consumed = true;
      } else if (item.subtype == 1) {  // Mana
        uint16_t mana = p.maxMp / 3;
        if (mana < 3) mana = 3;
        p.mp = std::min(static_cast<uint16_t>(p.mp + mana), p.maxMp);
        snprintf(msgBuf, sizeof(msgBuf), "Magical energy flows! (MP +%u)", mana);
        consumed = true;
      } else if (item.subtype == 2) {  // Strength
        p.strength += 2;
        snprintf(msgBuf, sizeof(msgBuf), "You feel stronger! (STR +2)");
        consumed = true;
      }
      break;

    case game::ItemType::Food:
      // Eating never costs a turn (this whole activity is turn-free), so it always fully
      // relieves hunger instantly and safely -- see processMonsterTurns() for the tick this
      // exists to counter, and the escapability guarantee that depends on this being turn-free.
      if (item.subtype == 0) {  // Rations
        uint16_t heal = 5;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), game::effectiveMaxHp(p));
        game::eatAndResetHunger(p.hunger);
        snprintf(msgBuf, sizeof(msgBuf), "That hit the spot. (HP +%u, hunger sated)", heal);
        consumed = true;
      } else if (item.subtype == 1) {  // Nutrient Bar
        uint16_t heal = game::effectiveMaxHp(p) / 2;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), game::effectiveMaxHp(p));
        uint16_t mana = p.maxMp / 2;
        p.mp = std::min(static_cast<uint16_t>(p.mp + mana), p.maxMp);
        game::eatAndResetHunger(p.hunger);
        snprintf(msgBuf, sizeof(msgBuf), "The System ration restores you!");
        consumed = true;
      }
      break;

    case game::ItemType::Scroll:
      if (item.subtype == 0) {  // Identify
        // Mark all inventory as identified
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          GAME_STATE.inventory[i].flags |= static_cast<uint8_t>(game::ItemFlag::Identified);
        }
        snprintf(msgBuf, sizeof(msgBuf), "Your items glow briefly.");
        consumed = true;
      } else if (item.subtype == 1) {  // Teleport
        // Handled back in game activity — for now just message
        snprintf(msgBuf, sizeof(msgBuf), "You blink! (use in dungeon)");
        consumed = true;
      } else if (item.subtype == 2) {  // Mapping
        snprintf(msgBuf, sizeof(msgBuf), "The level is revealed! (use in dungeon)");
        consumed = true;
      }
      break;

    case game::ItemType::LootBox: {
      // Sponsor Crate (Phase 11 loot boxes). Reuses ITEM_DEFS -- no second table. Uniform
      // draw over the eligible pool, deliberately NOT depth-biased (parent's explicit ruling,
      // 2026-08-16: a loot box that skews high with depth is just a good item with extra
      // steps -- the joke only lands if a deep floor can still hand back rations). Excludes
      // Ring of Power / Master Key (positional, same technique placeItems() uses in
      // DungeonGenerator.cpp) and the crate itself (LOOT_BOX_DEF, GameTypes.h) so opening one
      // can't just hand back another unopened crate. Selection itself lives in
      // game::selectLootBoxReward() (GameTypes.h) so it's host-harness-linkable.
      const auto& reward = game::ITEM_DEFS[game::selectLootBoxReward(
          [](uint32_t max) { return GAME_STATE.rollRange(max); })];

      // When sponsors land as their own work item, this narration takes the sponsor name from
      // that system instead of hardcoding "SPONSORED CONTENT" -- not building a second string
      // table now, per parent's explicit call.
      // Sponsor courtesy clause (Phase 11): extends the already-signed string above,
      // never edits it. Only appended when a real sponsor is active -- SPONSOR_NONE
      // (index 0, e.g. between floor loads) gets no clause at all.
      const char* sponsorName = game::SPONSOR_DEFS[p.activeSponsorId].name;
      bool hasSponsor = p.activeSponsorId != game::SPONSOR_NONE;

      if (reward.type == static_cast<uint8_t>(game::ItemType::Gold)) {
        int base = GAME_STATE.rollRangeInclusive(1, 10 + p.dungeonDepth * 5);
        int pct = game::sponsorGoldPercentModifier(p.activeSponsorId);
        uint16_t amount = static_cast<uint16_t>(base + (base * pct) / 100);
        p.gold += amount;
        if (hasSponsor) {
          snprintf(msgBuf, sizeof(msgBuf),
                   "SPONSORED CONTENT: Congratulations! You've won %u gold! (Terms apply. Terms are "
                   "unfavourable.) Brought to you by %s.",
                   amount, sponsorName);
        } else {
          snprintf(msgBuf, sizeof(msgBuf),
                   "SPONSORED CONTENT: Congratulations! You've won %u gold! (Terms apply. Terms are unfavourable.)",
                   amount);
        }
        consumed = true;  // reward went to the purse, not a slot -- box just disappears
      } else {
        item.type = reward.type;
        item.subtype = reward.subtype;
        item.count = 1;
        item.enchantment = 0;
        item.flags = 0;
        if (hasSponsor) {
          snprintf(msgBuf, sizeof(msgBuf), "SPONSORED CONTENT: Congratulations! You've won... %s! Brought to you by %s.",
                   reward.name, sponsorName);
        } else {
          snprintf(msgBuf, sizeof(msgBuf), "SPONSORED CONTENT: Congratulations! You've won... %s!", reward.name);
        }
        // consumed stays false: the crate transforms into its prize in place rather than being
        // removed-then-reinserted, so opening a crate can never fail on a full inventory (it
        // already held the slot a moment ago).
      }

      game::GameEvent boxEvent{};
      boxEvent.type = game::GameEventType::LootBoxOpened;
      ACHIEVEMENTS.emit(boxEvent);
      break;
    }

    default:
      // Weapons, armor, etc. — toggle equip
      if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
        item.flags &= ~static_cast<uint8_t>(game::ItemFlag::Equipped);
        snprintf(msgBuf, sizeof(msgBuf), "Unequipped.");
      } else {
        // Unequip any existing item of same type
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          if (static_cast<int>(i) != index && GAME_STATE.inventory[i].type == item.type &&
              (GAME_STATE.inventory[i].flags & static_cast<uint8_t>(game::ItemFlag::Equipped))) {
            GAME_STATE.inventory[i].flags &= ~static_cast<uint8_t>(game::ItemFlag::Equipped);
          }
        }
        item.flags |= static_cast<uint8_t>(game::ItemFlag::Equipped);
        snprintf(msgBuf, sizeof(msgBuf), "Equipped!");
      }
      break;
  }

  GAME_STATE.addMessage(msgBuf);

  if (consumed) {
    game::GameEvent itemEvent{};
    itemEvent.type = game::GameEventType::ItemUsed;
    itemEvent.itemType = static_cast<game::ItemType>(item.type);
    ACHIEVEMENTS.emit(itemEvent);

    // Remove item by shifting
    for (int i = index; i < GAME_STATE.inventoryCount - 1; i++) {
      GAME_STATE.inventory[i] = GAME_STATE.inventory[i + 1];
    }
    GAME_STATE.inventoryCount--;
    if (selectedIndex >= GAME_STATE.inventoryCount && selectedIndex > 0) {
      selectedIndex--;
    }
  }
}

// --- Rendering ---

void GameMenuActivity::render(RenderLock&&) {
  switch (currentScreen) {
    case Screen::Menu:
      renderMenu();
      break;
    case Screen::Inventory:
      renderInventory();
      break;
    case Screen::Character:
      renderCharacter();
      break;
    case Screen::Pet:
      renderPet();
      break;
    case Screen::Achievements:
      renderAchievements();
      break;
    case Screen::ThrowTarget:
      renderThrowTarget();
      break;
  }
}

void GameMenuActivity::renderMenu() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight), tr(STR_DM_MENU_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  static const StrId items[] = {StrId::STR_DM_RESUME_GAME,   StrId::STR_DM_INVENTORY,
                                StrId::STR_DM_CHARACTER,     StrId::STR_DM_PET,
                                StrId::STR_DM_ACHIEVEMENTS_TITLE,
                                StrId::STR_DM_THEME,         StrId::STR_DM_SAVE_AND_QUIT,
                                StrId::STR_DM_ABANDON_RUN};

  GUI.drawButtonMenu(
      renderer, Rect(0, contentTop, pageWidth, contentHeight), 8, selectedIndex,
      [](int index) {
        if (index == 5) {
          // Theme row shows the live selection, e.g. "Theme: Default" -- mirrors
          // the " +2"/" [E]" dynamic-suffix pattern used for inventory rows.
          const auto* theme = game::getTheme(static_cast<game::GameThemeId>(SETTINGS.gameTheme));
          return std::string(I18N.get(items[index])) + ": " + theme->name;
        }
        return std::string(I18N.get(items[index]));
      },
      nullptr);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void GameMenuActivity::renderInventory() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight), tr(STR_DM_INVENTORY));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  int invCount = static_cast<int>(GAME_STATE.inventoryCount);

  if (invCount == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_DM_PACK_EMPTY));
  } else {
    GUI.drawList(
        renderer, Rect(0, contentTop, pageWidth, contentHeight), invCount, selectedIndex,
        [](int index) {
          const auto& item = GAME_STATE.inventory[index];
          // Find the item name from definitions
          for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
            if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
              std::string name = game::ITEM_DEFS[d].name;
              if (item.enchantment > 0) {
                name += " +" + std::to_string(item.enchantment);
              }
              if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
                name += " [E]";
              }
              return name;
            }
          }
          return std::string("Unknown Item");
        },
        [](int index) {
          const auto& item = GAME_STATE.inventory[index];
          auto type = static_cast<game::ItemType>(item.type);
          switch (type) {
            case game::ItemType::Weapon:
              return std::string("Weapon");
            case game::ItemType::Armor:
              return std::string("Armor");
            case game::ItemType::Shield:
              return std::string("Shield");
            case game::ItemType::Potion:
              return std::string("Potion");
            case game::ItemType::Scroll:
              return std::string("Scroll");
            case game::ItemType::Food:
              return std::string("Food");
            case game::ItemType::Ring:
              return std::string("Ring");
            case game::ItemType::Amulet:
              return std::string("Amulet");
            default:
              return std::string("");
          }
        },
        nullptr,
        [](int index) {
          const auto& item = GAME_STATE.inventory[index];
          for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
            if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
              int atk = game::ITEM_DEFS[d].attack + item.enchantment;
              int def = game::ITEM_DEFS[d].defense + item.enchantment;
              if (atk > 0) {
                return "+" + std::to_string(atk) + " ATK";
              }
              if (def > 0) {
                return "+" + std::to_string(def) + " DEF";
              }
              if (item.count > 1) {
                return "x" + std::to_string(item.count);
              }
              return std::string("");
            }
          }
          return std::string("");
        });
  }

  const char* confirmLabel = invCount > 0 ? tr(STR_DM_USE_EQUIP) : "";

  // Surface the hold-to-throw hint only when the hovered item is actually throwable --
  // otherwise the fourth hint slot stays blank, matching every other screen's convention
  // of not showing a hint for an action that isn't currently available.
  const char* holdThrowLabel = "";
  if (invCount > 0) {
    const auto* def = findItemDef(GAME_STATE.inventory[selectedIndex]);
    if (def != nullptr && def->throwable) {
      holdThrowLabel = tr(STR_DM_HINT_HOLD_THROW);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, "", holdThrowLabel);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void GameMenuActivity::renderCharacter() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight), tr(STR_DM_CHARACTER));

  const auto& p = GAME_STATE.player;
  const int x = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 10;
  constexpr int lineH = 28;

  char buf[64];

  snprintf(buf, sizeof(buf), "Level: %u", p.charLevel);
  renderer.drawText(UI_10_FONT_ID, x, y, buf, true, EpdFontFamily::BOLD);
  y += lineH;

  snprintf(buf, sizeof(buf), "HP: %u / %u", p.hp, game::effectiveMaxHp(p));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "MP: %u / %u", p.mp, p.maxMp);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  y += 10;  // spacer

  snprintf(buf, sizeof(buf), "Strength:     %u", p.strength);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Dexterity:    %u", p.dexterity);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Constitution: %u", p.constitution);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Intelligence: %u", p.intelligence);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  y += 10;  // spacer

  // What the gear is actually doing (Stuart's ask): the real combat formulas,
  // base stat + equipped bonus (weapons/armor/shields + active sponsor -- see
  // game::equippedAttackBonus()/equippedDefenseBonus() in GameState.h), not a
  // rewrite of the base stat itself.
  int atkBonus = game::equippedAttackBonus();
  int atkPower = static_cast<int>(p.strength) + atkBonus;
  snprintf(buf, sizeof(buf), "Attack Power: %d (%u %+d)", atkPower, p.strength, atkBonus);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  int defBonus = game::equippedDefenseBonus();
  int playerDef = static_cast<int>(p.dexterity / 3) + defBonus;
  snprintf(buf, sizeof(buf), "Defense: %d (%u %+d)", playerDef, static_cast<unsigned>(p.dexterity / 3), defBonus);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  y += 10;  // spacer

  snprintf(buf, sizeof(buf), "Experience: %lu", static_cast<unsigned long>(p.experience));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  uint32_t nextLvlXp = game::xpForLevel(p.charLevel + 1);
  uint32_t xpRemaining = (nextLvlXp > p.experience) ? (nextLvlXp - p.experience) : 0;
  snprintf(buf, sizeof(buf), "Next level: %lu XP", static_cast<unsigned long>(xpRemaining));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Gold: %u", p.gold);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Dungeon depth: %u", p.dungeonDepth);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Turns: %u", p.turnCount);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);

  // Show equipped gear at bottom
  y += lineH + 10;
  renderer.drawText(UI_10_FONT_ID, x, y, "Equipment:", true, EpdFontFamily::BOLD);
  y += lineH;

  bool hasEquipped = false;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          std::string name = game::ITEM_DEFS[d].name;
          if (item.enchantment > 0) {
            name += " +" + std::to_string(item.enchantment);
          }
          int atk = game::ITEM_DEFS[d].attack + item.enchantment;
          int def = game::ITEM_DEFS[d].defense + item.enchantment;
          if (atk > 0) {
            name += "  +" + std::to_string(atk) + " ATK";
          } else if (def > 0) {
            name += "  +" + std::to_string(def) + " DEF";
          }
          renderer.drawText(UI_10_FONT_ID, x + 10, y, name.c_str());
          y += lineH;
          hasEquipped = true;
          break;
        }
      }
    }
  }
  if (!hasEquipped) {
    renderer.drawText(UI_10_FONT_ID, x + 10, y, "(none)");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void GameMenuActivity::renderPet() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight), tr(STR_DM_PET));

  const auto& pet = GAME_STATE.pet;
  const int x = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 10;
  constexpr int lineH = 28;

  if (!pet.active) {
    renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_DM_PET_NO_COMPANION));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  char buf[64];

  renderer.drawText(UI_10_FONT_ID, x, y, pet.name, true, EpdFontFamily::BOLD);
  y += lineH;

  const auto& species = game::PET_SPECIES_DEFS[pet.speciesId];
  snprintf(buf, sizeof(buf), "%s %s", I18N.get(StrId::STR_DM_PET_SPECIES), species.name);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  y += 10;  // spacer

  snprintf(buf, sizeof(buf), "%s %u", I18N.get(StrId::STR_DM_PET_LEVEL), game::petLevel());
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "%s %u", I18N.get(StrId::STR_DM_PET_HP), game::petMaxHp(pet));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "%s %d", I18N.get(StrId::STR_DM_PET_ATTACK), game::petAttack(pet));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "%s %d", I18N.get(StrId::STR_DM_PET_DEFENSE), game::petDefense(pet));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  y += 10;  // spacer

  renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_DM_PET_GEAR), true, EpdFontFamily::BOLD);
  y += lineH;

  if (pet.hasGear) {
    const auto* def = game::petFindItemDef(pet.gear);
    std::string name = def != nullptr ? def->name : "?";
    if (pet.gear.enchantment > 0) {
      name += " +" + std::to_string(pet.gear.enchantment);
    }
    renderer.drawText(UI_10_FONT_ID, x + 10, y, name.c_str());
  } else {
    renderer.drawText(UI_10_FONT_ID, x + 10, y, tr(STR_DM_PET_NO_GEAR));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void GameMenuActivity::renderAchievements() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight),
                 tr(STR_DM_ACHIEVEMENTS_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Lifetime unlock state (not per-run) -- this is an account-wide trophy case, unlike the
  // this-run-only list on the death/victory screen. Per Stuart's explicit call (2026-08-17):
  // locked achievements are never shown at all, not even redacted -- only unlocked ones
  // appear, each with its name, description, and reward effect spelled out.
  uint8_t unlockedIds[static_cast<uint8_t>(game::AchievementId::Count)];
  int unlockedCount = 0;
  for (uint8_t i = 0; i < static_cast<uint8_t>(game::AchievementId::Count); i++) {
    if (ACHIEVEMENTS.isUnlocked(static_cast<game::AchievementId>(i))) {
      unlockedIds[unlockedCount++] = i;
    }
  }

  char countBuf[32];
  snprintf(countBuf, sizeof(countBuf), "%d / %d discovered", unlockedCount,
           static_cast<int>(game::AchievementId::Count));

  if (unlockedCount == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, countBuf);
  } else {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop, countBuf);

    constexpr int countLineH = 24;
    const int listTop = contentTop + countLineH;
    const int listHeight = contentHeight - countLineH;

    // achievementDef() is bounds-safe past ACHIEVEMENT_DEF_COUNT (the
    // data-driven pool from IronStomach/48 onward has no def entry yet); its
    // name field is empty for those ids, so fall back to achievementShortName
    // for display rather than showing a blank row.
    GUI.drawList(
        renderer, Rect(0, listTop, pageWidth, listHeight), unlockedCount, selectedIndex,
        [&unlockedIds](int index) {
          auto id = static_cast<game::AchievementId>(unlockedIds[index]);
          const auto& def = game::achievementDef(id);
          return std::string(def.name[0] != '\0' ? def.name : game::achievementShortName(id));
        },
        [&unlockedIds](int index) {
          const auto& def = game::achievementDef(static_cast<game::AchievementId>(unlockedIds[index]));
          std::string subtitle = def.description;
          char rewardBuf[64];
          game::achievementRewardText(def, rewardBuf, sizeof(rewardBuf));
          if (rewardBuf[0] != '\0') {
            subtitle += " -- ";
            subtitle += rewardBuf;
          }
          return subtitle;
        },
        nullptr, nullptr);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void GameMenuActivity::renderThrowTarget() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect(0, metrics.topPadding, pageWidth, metrics.headerHeight), tr(STR_DM_THROW));

  const int x = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 10;
  constexpr int lineH = 28;

  // Name the item actually in flight -- throwItemIndex was captured on long-press,
  // not re-derived from selectedIndex (which Screen::Inventory keeps mutating).
  if (throwItemIndex >= 0 && throwItemIndex < GAME_STATE.inventoryCount) {
    const auto* def = findItemDef(GAME_STATE.inventory[throwItemIndex]);
    if (def != nullptr) {
      renderer.drawText(UI_10_FONT_ID, x, y, def->name, true, EpdFontFamily::BOLD);
      y += lineH;
    }
  }

  renderer.drawText(UI_10_FONT_ID, x, y, tr(STR_DM_THROW_PROMPT));

  const auto labels = mappedInput.mapDirectionalLabels(tr(STR_BACK), "", tr(STR_DM_HINT_LEFT), tr(STR_DM_HINT_RIGHT),
                                                        tr(STR_DM_HINT_UP), tr(STR_DM_HINT_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
