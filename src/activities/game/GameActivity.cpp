#include "GameActivity.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstring>

#include "GameMenuActivity.h"
#include "MappedInputManager.h"
#include "activities/ActivityResult.h"
#include "game/AchievementBus.h"
#include "game/FlavorText.h"
#include "game/GameSave.h"

// --- FOV ---

namespace {
constexpr int FOV_RADIUS = 8;

// Bresenham line-of-sight: returns true if no wall blocks the line from (x0,y0) to (x1,y1)
bool hasLineOfSight(const game::Tile* tiles, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  int cx = x0, cy = y0;
  while (true) {
    // Skip the starting tile check
    if ((cx != x0 || cy != y0) && (cx != x1 || cy != y1)) {
      if (tiles[cy * game::MAP_WIDTH + cx] == game::Tile::Wall) {
        return false;
      }
    }
    if (cx == x1 && cy == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      cx += sx;
    }
    if (e2 <= dx) {
      err += dx;
      cy += sy;
    }
  }
  return true;
}

// Calculate total attack bonus from equipped weapons
int equippedAttackBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          bonus += game::ITEM_DEFS[d].attack + item.enchantment;
          break;
        }
      }
    }
  }
  return bonus;
}

// Calculate total defense bonus from equipped armor/shields
int equippedDefenseBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          bonus += game::ITEM_DEFS[d].defense + item.enchantment;
          break;
        }
      }
    }
  }
  return bonus;
}

// Check if a tile is walkable for monsters
bool isWalkable(game::Tile tile) {
  return tile == game::Tile::Floor || tile == game::Tile::DoorOpen || tile == game::Tile::StairsUp ||
         tile == game::Tile::StairsDown || tile == game::Tile::Rubble;
}

}  // namespace

// --- Lifecycle ---

void GameActivity::onEnter() {
  Activity::onEnter();

  gameRenderer.init(renderer);

  // Achievement unlock state is account-level, not tied to the per-run save.
  ACHIEVEMENTS.load();

  // Load save or start new game
  if (GAME_STATE.hasSaveFile()) {
    GAME_STATE.loadFromFile();
  }
  // If no save was loaded, newGame() was already called before entering this activity
  // (see GameTitleActivity::loop()).

  loadOrGenerateLevel();
  computeVisibility();
  requestUpdate();
}

// --- Render ---

void GameActivity::render(RenderLock&&) {
  if (screenMode != GameScreenMode::Playing) {
    gameRenderer.drawEndScreen(renderer, screenMode == GameScreenMode::Victory, endScreenData);
    return;
  }
  gameRenderer.draw(renderer, tiles, fogOfWar, monsters, monsterCount, levelItems, itemCount, visible);
}

// --- Input ---

void GameActivity::loop() {
  using Button = MappedInputManager::Button;

  if (screenMode != GameScreenMode::Playing) {
    // Blocking death/victory screen: tap-dismiss only (Phase 7 req 2/3) --
    // any button release or screen tap completes the navigation that
    // handlePlayerDeath()/handleVictory() deferred. Save deletion already
    // happened at that point, so this is purely "leave the screen".
    bool dismissed = mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Down) ||
                     mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Right) ||
                     mappedInput.wasReleased(Button::Confirm) || mappedInput.wasReleased(Button::Back);
    if (!dismissed) {
      int tx, ty;
      dismissed = mappedInput.wasScreenTapped(tx, ty);
    }
    if (dismissed) {
      onGoHome();
    }
    return;
  }

  Button pressed = Button::Confirm;
  bool hasButton = false;

  if (mappedInput.wasReleased(Button::Up)) {
    pressed = Button::Up;
    hasButton = true;
  } else if (mappedInput.wasReleased(Button::Down)) {
    pressed = Button::Down;
    hasButton = true;
  } else if (mappedInput.wasReleased(Button::Left)) {
    pressed = Button::Left;
    hasButton = true;
  } else if (mappedInput.wasReleased(Button::Right)) {
    pressed = Button::Right;
    hasButton = true;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    pressed = Button::Confirm;
    hasButton = true;
  } else if (mappedInput.wasReleased(Button::Back)) {
    pressed = Button::Back;
    hasButton = true;
  } else {
    // On-screen touch controls: the control area doubles as a d-pad + Action/Menu
    // tap surface. Routed through the same handleMove/handleAction/openGameMenu
    // calls as physical buttons, so there's exactly one dispatch path either way.
    int tx, ty;
    if (mappedInput.wasScreenTapped(tx, ty) && gameRenderer.hitTestControls(tx, ty, pressed)) {
      hasButton = true;
    }
  }

  if (!hasButton) return;

  switch (pressed) {
    case Button::Up:
      handleMove(0, -1);
      break;
    case Button::Down:
      handleMove(0, 1);
      break;
    case Button::Left:
      handleMove(-1, 0);
      break;
    case Button::Right:
      handleMove(1, 0);
      break;
    case Button::Confirm:
      handleAction();
      break;
    case Button::Back:
      openGameMenu();
      break;
    default:
      break;
  }
}

// --- Game Menu ---

void GameActivity::openGameMenu() {
  startActivityForResult(std::make_unique<GameMenuActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onGameMenuResult(result); });
}

bool GameActivity::handleHomeGesture() {
  openGameMenu();
  return true;
}

void GameActivity::onGameMenuResult(const ActivityResult& result) {
  const auto* menuResult = std::get_if<MenuResult>(&result.data);
  auto action =
      menuResult ? static_cast<GameMenuActivity::MenuAction>(menuResult->action) : GameMenuActivity::MenuAction::RESUME;

  switch (action) {
    case GameMenuActivity::MenuAction::RESUME:
      requestUpdate();
      return;

    case GameMenuActivity::MenuAction::SAVE_QUIT:
      saveCurrentLevel();
      GAME_STATE.saveToFile();
      GAME_STATE.addMessage("Game saved.");
      onGoHome();
      return;

    case GameMenuActivity::MenuAction::ABANDON:
      GameSave::deleteAll();
      GAME_STATE.deleteSaveFile();
      onGoHome();
      return;

    case GameMenuActivity::MenuAction::THROW:
      handleThrow(static_cast<game::Direction>(menuResult->orientation),
                  static_cast<int>(menuResult->pageTurnOption));
      return;
  }
}

// --- Movement ---

void GameActivity::handleMove(int dx, int dy) {
  auto& p = GAME_STATE.player;

  // Dead players don't move
  if (p.hp == 0) return;

  int newX = p.x + dx;
  int newY = p.y + dy;

  // Bounds check
  if (newX < 0 || newX >= game::MAP_WIDTH || newY < 0 || newY >= game::MAP_HEIGHT) return;

  game::Tile target = tiles[newY * game::MAP_WIDTH + newX];

  // Wall blocks movement
  if (target == game::Tile::Wall) return;
  if (target == game::Tile::DoorClosed) {
    // Open the door
    tiles[newY * game::MAP_WIDTH + newX] = game::Tile::DoorOpen;
    game::fogSetExplored(doorOpen, newX, newY);  // persist across save/reload (Phase 7 req 6)
    GAME_STATE.addMessage("You open the door.");
    p.turnCount++;
    if (processMonsterTurns()) {
      handlePlayerDeath();
      return;
    }
    computeVisibility();
    requestUpdate();
    return;
  }

  // Check for monster at target position
  for (uint8_t i = 0; i < monsterCount; i++) {
    if (monsters[i].x == newX && monsters[i].y == newY && monsters[i].hp > 0) {
      // Melee attack (strength + weapon bonus vs monster defense)
      const auto& def = game::MONSTER_DEFS[monsters[i].type];
      int atkPower = static_cast<int>(p.strength) + equippedAttackBonus();
      int damage = std::max(1, atkPower - static_cast<int>(def.defense));
      // Add some variance
      damage = std::max(1, damage + GAME_STATE.rollRangeInclusive(-damage / 4, damage / 4));

      monsters[i].hp = (damage >= monsters[i].hp) ? 0 : monsters[i].hp - damage;

      char msgBuf[96];
      if (monsters[i].hp == 0) {
        const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::MonsterKilled);
        snprintf(msgBuf, sizeof(msgBuf), "You slay the %s! (+%uXP) %s", def.name, def.expValue, flavor);
        GAME_STATE.addMessage(msgBuf);
        p.experience += def.expValue;
        p.kills++;

        game::GameEvent killEvent{};
        killEvent.type = game::GameEventType::MonsterKilled;
        killEvent.damage = static_cast<uint16_t>(damage);
        killEvent.monsterMaxHp = def.baseHp;
        ACHIEVEMENTS.emit(killEvent);
        if (ACHIEVEMENTS.hasNewUnlock()) {
          gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
        }

        checkLevelUp();

        // Boss drops the Ring of Power
        if (monsters[i].type == game::BOSS_MONSTER_TYPE && itemCount < game::MAX_ITEMS_PER_LEVEL) {
          auto& ring = levelItems[itemCount];
          ring.x = monsters[i].x;
          ring.y = monsters[i].y;
          ring.type = game::ITEM_DEFS[game::RING_OF_POWER_DEF].type;
          ring.subtype = game::ITEM_DEFS[game::RING_OF_POWER_DEF].subtype;
          ring.count = 1;
          ring.enchantment = 0;
          ring.flags = 0;
          itemCount++;
          GAME_STATE.addMessage("Something glints on the ground...");
        }
      } else {
        auto band = game::hitBandForDamage(static_cast<uint16_t>(damage), def.baseHp);
        const char* flavor = FLAVOR_TEXT.pick(band);
        snprintf(msgBuf, sizeof(msgBuf), "You hit the %s for %d. %s", def.name, damage, flavor);
        GAME_STATE.addMessage(msgBuf);
      }

      // Monster becomes hostile
      monsters[i].state = static_cast<uint8_t>(game::MonsterState::Hostile);

      p.turnCount++;
      if (processMonsterTurns()) {
        handlePlayerDeath();
        return;
      }
      computeVisibility();
      requestUpdate();
      return;
    }
  }

  // Move player
  p.x = static_cast<int16_t>(newX);
  p.y = static_cast<int16_t>(newY);
  p.turnCount++;

  if (processMonsterTurns()) {
    handlePlayerDeath();
    return;
  }
  computeVisibility();
  requestUpdate();
}

// --- Action (Confirm button) ---

void GameActivity::handleAction() {
  auto& p = GAME_STATE.player;

  // Defensive fallback only -- loop()'s screenMode gate normally stops input
  // from reaching handleAction() at all once dead. handlePlayerDeath() is
  // idempotent, so a redundant call here is harmless.
  if (p.hp == 0) {
    handlePlayerDeath();
    return;
  }

  int mapIdx = p.y * game::MAP_WIDTH + p.x;
  game::Tile here = tiles[mapIdx];

  // Stairs
  if (here == game::Tile::StairsDown) {
    if (p.dungeonDepth >= game::MAX_DEPTH) {
      GAME_STATE.addMessage("The stairs are blocked by rubble.");
      requestUpdate();
      return;
    }
    saveCurrentLevel();
    p.dungeonDepth++;

    game::GameEvent floorEvent{};
    floorEvent.type = game::GameEventType::FloorChanged;
    ACHIEVEMENTS.emit(floorEvent);
    if (ACHIEVEMENTS.hasNewUnlock()) {
      gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
    }

    {
      char msgBuf[96];
      const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::FloorEntry);
      snprintf(msgBuf, sizeof(msgBuf), "You descend deeper... %s", flavor);
      GAME_STATE.addMessage(msgBuf);
      gameRenderer.showNotification(NotificationKind::FloorEntry, msgBuf);
    }
    loadOrGenerateLevel();

    // Boss-floor arrival: the boss monster type is always seeded on the deepest
    // level (see game::BOSS_MONSTER_TYPE) -- surface it as its own System beat.
    for (int i = 0; i < monsterCount; i++) {
      if (monsters[i].type == game::BOSS_MONSTER_TYPE) {
        char msgBuf[96];
        const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::BossArrival);
        snprintf(msgBuf, sizeof(msgBuf), "%s", flavor);
        GAME_STATE.addMessage(msgBuf);
        gameRenderer.showNotification(NotificationKind::BossArrival, msgBuf);
        break;
      }
    }

    // Place player at stairs up on new level
    for (int i = 0; i < game::MAP_SIZE; i++) {
      if (tiles[i] == game::Tile::StairsUp) {
        p.x = static_cast<int16_t>(i % game::MAP_WIDTH);
        p.y = static_cast<int16_t>(i / game::MAP_WIDTH);
        break;
      }
    }
    computeVisibility();
    requestUpdate();
    return;
  }

  if (here == game::Tile::StairsUp) {
    if (p.dungeonDepth <= 1) {
      GAME_STATE.addMessage("You see daylight above... but the mines call.");
      requestUpdate();
      return;
    }
    saveCurrentLevel();
    p.dungeonDepth--;
    GAME_STATE.addMessage("You ascend...");
    loadOrGenerateLevel();
    // Place player at stairs down on previous level
    for (int i = 0; i < game::MAP_SIZE; i++) {
      if (tiles[i] == game::Tile::StairsDown) {
        p.x = static_cast<int16_t>(i % game::MAP_WIDTH);
        p.y = static_cast<int16_t>(i / game::MAP_WIDTH);
        break;
      }
    }
    computeVisibility();
    requestUpdate();
    return;
  }

  // Pick up items
  for (uint8_t i = 0; i < itemCount; i++) {
    if (levelItems[i].x == p.x && levelItems[i].y == p.y) {
      // Check if this is the Ring of Power — victory!
      const auto& ringDef = game::ITEM_DEFS[game::RING_OF_POWER_DEF];
      if (levelItems[i].type == ringDef.type && levelItems[i].subtype == ringDef.subtype) {
        GAME_STATE.addMessage("You claim the Ring of Power!");
        GAME_STATE.addMessage("The mines tremble... You have won!");
        handleVictory();
        return;
      }

      // Gold goes straight to purse
      if (static_cast<game::ItemType>(levelItems[i].type) == game::ItemType::Gold) {
        uint16_t amount = levelItems[i].count * 10;
        p.gold += amount;
        char msgBuf[48];
        snprintf(msgBuf, sizeof(msgBuf), "You pick up %u gold.", amount);
        GAME_STATE.addMessage(msgBuf);
      } else {
        if (GAME_STATE.inventoryCount >= game::MAX_INVENTORY) {
          GAME_STATE.addMessage("Your pack is full!");
          requestUpdate();
          return;
        }
        // Move item to inventory
        GAME_STATE.inventory[GAME_STATE.inventoryCount] = levelItems[i];
        GAME_STATE.inventory[GAME_STATE.inventoryCount].x = -1;
        GAME_STATE.inventory[GAME_STATE.inventoryCount].y = -1;
        GAME_STATE.inventoryCount++;

        // Find matching item def for message
        char msgBuf[64];
        for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
          if (game::ITEM_DEFS[d].type == levelItems[i].type && game::ITEM_DEFS[d].subtype == levelItems[i].subtype) {
            snprintf(msgBuf, sizeof(msgBuf), "You pick up %s.", game::ITEM_DEFS[d].name);
            GAME_STATE.addMessage(msgBuf);
            break;
          }
        }

        game::GameEvent pickupEvent{};
        pickupEvent.type = game::GameEventType::ItemPickedUp;
        ACHIEVEMENTS.emit(pickupEvent);
        if (ACHIEVEMENTS.hasNewUnlock()) {
          gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
        }
      }

      // Remove from level by swapping with last
      levelItems[i] = levelItems[itemCount - 1];
      itemCount--;

      p.turnCount++;
      if (processMonsterTurns()) {
        handlePlayerDeath();
        return;
      }
      requestUpdate();
      return;
    }
  }

  GAME_STATE.addMessage("Nothing to do here.");
  requestUpdate();
}

// --- Throw ---

void GameActivity::handleThrow(game::Direction dir, int inventoryIndex) {
  auto& p = GAME_STATE.player;

  if (p.hp == 0) return;
  if (inventoryIndex < 0 || inventoryIndex >= GAME_STATE.inventoryCount) return;

  const auto& item = GAME_STATE.inventory[inventoryIndex];
  const game::ItemDef* def = nullptr;
  for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
    if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
      def = &game::ITEM_DEFS[d];
      break;
    }
  }
  if (def == nullptr || !def->throwable) return;

  int dx = 0, dy = 0;
  switch (dir) {
    case game::Direction::North: dy = -1; break;
    case game::Direction::South: dy = 1; break;
    case game::Direction::East:  dx = 1; break;
    case game::Direction::West:  dx = -1; break;
  }

  // Walk the line from the player outward, stopping at the first wall or the first
  // monster encountered -- "nearest monster in line" (Phase 10 work item 2), not a
  // full-line AoE.
  int targetIdx = -1;
  int cx = p.x + dx;
  int cy = p.y + dy;
  while (cx >= 0 && cx < game::MAP_WIDTH && cy >= 0 && cy < game::MAP_HEIGHT) {
    if (tiles[cy * game::MAP_WIDTH + cx] == game::Tile::Wall) break;
    for (uint8_t i = 0; i < monsterCount; i++) {
      if (monsters[i].hp > 0 && monsters[i].x == cx && monsters[i].y == cy) {
        targetIdx = i;
        break;
      }
    }
    if (targetIdx != -1) break;
    cx += dx;
    cy += dy;
  }

  // Consume the thrown item regardless of hit/miss -- it's gone once it leaves your
  // hand, same as the item disappearing off a monster's death drop. Stacked items
  // (count > 1) lose one from the stack; a single-count item is removed outright,
  // same shift-removal idiom useInventoryItem() uses for consumables.
  bool killedMonster = false;
  char msgBuf[96];

  if (targetIdx == -1) {
    snprintf(msgBuf, sizeof(msgBuf), "You throw the %s. It clatters away.", def->name);
    GAME_STATE.addMessage(msgBuf);
  } else {
    auto& mon = monsters[targetIdx];
    const auto& monDef = game::MONSTER_DEFS[mon.type];

    // Dexterity-based curve, deliberately distinct from melee's strength-based one
    // (Phase 10 requirement 2): half dexterity (a thrown weapon leans on precision,
    // not raw power) plus the item's own attack/enchantment values, minus monster
    // defense, with wider (+-33%) variance than melee's +-25% -- a thrown item is
    // less consistent than a wielded one.
    int atkPower = static_cast<int>(p.dexterity) / 2 + def->attack + item.enchantment;
    int damage = std::max(1, atkPower - static_cast<int>(monDef.defense));
    damage = std::max(1, damage + GAME_STATE.rollRangeInclusive(-damage / 3, damage / 3));

    mon.hp = (damage >= mon.hp) ? 0 : mon.hp - damage;
    killedMonster = (mon.hp == 0);

    if (killedMonster) {
      const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::MonsterKilled);
      snprintf(msgBuf, sizeof(msgBuf), "Your thrown %s slays the %s! (+%uXP) %s", def->name, monDef.name,
               monDef.expValue, flavor);
      GAME_STATE.addMessage(msgBuf);
      p.experience += monDef.expValue;
      p.kills++;
      checkLevelUp();

      // Boss drops the Ring of Power -- same parity as a melee boss kill (see the
      // handleMove() kill branch above); a thrown kill must not be able to softlock
      // progression by skipping the drop.
      if (mon.type == game::BOSS_MONSTER_TYPE && itemCount < game::MAX_ITEMS_PER_LEVEL) {
        auto& ring = levelItems[itemCount];
        ring.x = mon.x;
        ring.y = mon.y;
        ring.type = game::ITEM_DEFS[game::RING_OF_POWER_DEF].type;
        ring.subtype = game::ITEM_DEFS[game::RING_OF_POWER_DEF].subtype;
        ring.count = 1;
        ring.enchantment = 0;
        ring.flags = 0;
        itemCount++;
        GAME_STATE.addMessage("Something glints on the ground...");
      }
    } else {
      auto band = game::hitBandForDamage(static_cast<uint16_t>(damage), monDef.baseHp);
      const char* flavor = FLAVOR_TEXT.pick(band);
      snprintf(msgBuf, sizeof(msgBuf), "Your thrown %s hits the %s for %d. %s", def->name, monDef.name, damage,
               flavor);
      GAME_STATE.addMessage(msgBuf);
    }

    mon.state = static_cast<uint8_t>(game::MonsterState::Hostile);
  }

  if (item.count > 1) {
    GAME_STATE.inventory[inventoryIndex].count--;
  } else {
    for (int i = inventoryIndex; i < GAME_STATE.inventoryCount - 1; i++) {
      GAME_STATE.inventory[i] = GAME_STATE.inventory[i + 1];
    }
    GAME_STATE.inventoryCount--;
  }

  game::GameEvent throwEvent{};
  throwEvent.type = game::GameEventType::ItemThrown;
  throwEvent.killedMonster = killedMonster;
  ACHIEVEMENTS.emit(throwEvent);
  if (ACHIEVEMENTS.hasNewUnlock()) {
    gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
  }

  p.turnCount++;
  if (processMonsterTurns()) {
    handlePlayerDeath();
    return;
  }
  computeVisibility();
  requestUpdate();
}

// --- Monster AI ---

bool GameActivity::processMonsterTurns() {
  auto& p = GAME_STATE.player;
  if (p.hp == 0) return true;

  for (uint8_t i = 0; i < monsterCount; i++) {
    auto& m = monsters[i];
    if (m.hp == 0) continue;

    int dx = static_cast<int>(p.x) - m.x;
    int dy = static_cast<int>(p.y) - m.y;
    int dist2 = dx * dx + dy * dy;

    // State transitions
    auto state = static_cast<game::MonsterState>(m.state);

    if (state == game::MonsterState::Asleep) {
      // Wake up if player is nearby and visible
      if (dist2 <= FOV_RADIUS * FOV_RADIUS && visible[m.y * game::MAP_WIDTH + m.x]) {
        // Wake chance based on distance — closer = more likely
        int wakeChance = 80 - dist2;  // Very likely when close
        if (static_cast<int>(GAME_STATE.rollRange(100)) < wakeChance) {
          m.state = static_cast<uint8_t>(game::MonsterState::Hostile);
          state = game::MonsterState::Hostile;
        }
      }
      continue;  // Asleep monsters don't act
    }

    if (state == game::MonsterState::Wandering) {
      // Become hostile if player is visible and close
      if (dist2 <= FOV_RADIUS * FOV_RADIUS && visible[m.y * game::MAP_WIDTH + m.x]) {
        m.state = static_cast<uint8_t>(game::MonsterState::Hostile);
        state = game::MonsterState::Hostile;
      } else {
        // Random wander
        int dir = static_cast<int>(GAME_STATE.rollRange(4));
        int wmx = m.x + ((dir == 0) ? 1 : (dir == 1) ? -1 : 0);
        int wmy = m.y + ((dir == 2) ? 1 : (dir == 3) ? -1 : 0);

        if (wmx >= 0 && wmx < game::MAP_WIDTH && wmy >= 0 && wmy < game::MAP_HEIGHT) {
          if (isWalkable(tiles[wmy * game::MAP_WIDTH + wmx])) {
            // Don't walk onto player or other monsters
            bool blocked = (wmx == p.x && wmy == p.y);
            if (!blocked) {
              for (uint8_t j = 0; j < monsterCount && !blocked; j++) {
                if (j != i && monsters[j].hp > 0 && monsters[j].x == wmx && monsters[j].y == wmy) {
                  blocked = true;
                }
              }
            }
            if (!blocked) {
              m.x = static_cast<int16_t>(wmx);
              m.y = static_cast<int16_t>(wmy);
            }
          }
        }
        continue;
      }
    }

    // Hostile: move toward player or attack
    if (state == game::MonsterState::Hostile) {
      // Adjacent to player? Attack!
      if (abs(dx) <= 1 && abs(dy) <= 1 && dist2 <= 2) {
        monsterAttackPlayer(m);
        if (p.hp == 0) return true;  // Player died
        continue;
      }

      // Move toward player (simple greedy pathfinding)
      int bestX = m.x;
      int bestY = m.y;
      int bestDist = dist2;

      // Try the 4 cardinal directions
      static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& d : dirs) {
        int nx = m.x + d[0];
        int ny = m.y + d[1];

        if (nx < 0 || nx >= game::MAP_WIDTH || ny < 0 || ny >= game::MAP_HEIGHT) continue;
        if (!isWalkable(tiles[ny * game::MAP_WIDTH + nx])) continue;

        // Don't walk onto player (would need to attack instead, handled above)
        if (nx == p.x && ny == p.y) continue;

        // Don't walk onto other monsters
        bool occupied = false;
        for (uint8_t j = 0; j < monsterCount; j++) {
          if (j != i && monsters[j].hp > 0 && monsters[j].x == nx && monsters[j].y == ny) {
            occupied = true;
            break;
          }
        }
        if (occupied) continue;

        int ndx = static_cast<int>(p.x) - nx;
        int ndy = static_cast<int>(p.y) - ny;
        int nd = ndx * ndx + ndy * ndy;
        if (nd < bestDist) {
          bestDist = nd;
          bestX = nx;
          bestY = ny;
        }
      }

      m.x = static_cast<int16_t>(bestX);
      m.y = static_cast<int16_t>(bestY);
    }
  }

  // Natural regeneration: heal 1 HP every (20 - CON/2) turns, minimum every 5 turns
  int regenRate = std::max(5, 20 - static_cast<int>(p.constitution) / 2);
  if (p.hp > 0 && p.hp < p.maxHp && p.turnCount % regenRate == 0) {
    p.hp++;
  }
  // MP regenerates a bit slower
  if (p.mp < p.maxMp && p.turnCount % (regenRate + 5) == 0) {
    p.mp++;
  }

  return false;
}

void GameActivity::monsterAttackPlayer(game::Monster& m) {
  auto& p = GAME_STATE.player;
  const auto& def = game::MONSTER_DEFS[m.type];

  // Monster attack vs player dexterity + armor bonus
  int playerDef = static_cast<int>(p.dexterity / 3) + equippedDefenseBonus();
  int damage = std::max(1, static_cast<int>(def.attack) - playerDef);
  damage = std::max(1, damage + GAME_STATE.rollRangeInclusive(-damage / 4, damage / 4));

  // Apply damage
  p.hp = (static_cast<uint16_t>(damage) >= p.hp) ? 0 : p.hp - static_cast<uint16_t>(damage);

  char msgBuf[96];
  if (p.hp == 0) {
    const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::PlayerDeath);
    snprintf(msgBuf, sizeof(msgBuf), "The %s kills you! %s", def.name, flavor);
    GAME_STATE.addMessage(msgBuf);
    snprintf(deathCause, sizeof(deathCause), "%s", def.name);
    gameRenderer.showNotification(NotificationKind::Death, msgBuf);

    game::GameEvent deathEvent{};
    deathEvent.type = game::GameEventType::PlayerDied;
    deathEvent.monsterMaxHp = def.baseHp;
    deathEvent.monsterAttack = def.attack;
    ACHIEVEMENTS.emit(deathEvent);
    if (ACHIEVEMENTS.hasNewUnlock()) {
      gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
    }
  } else {
    auto band = game::damagedBandForDamage(static_cast<uint16_t>(damage), p.maxHp);
    const char* flavor = FLAVOR_TEXT.pick(band);
    snprintf(msgBuf, sizeof(msgBuf), "The %s hits you for %d. %s", def.name, damage, flavor);
    GAME_STATE.addMessage(msgBuf);
  }

  game::GameEvent damageEvent{};
  damageEvent.type = game::GameEventType::PlayerDamaged;
  damageEvent.damage = static_cast<uint16_t>(damage);
  damageEvent.hpAfter = p.hp;
  damageEvent.maxHp = p.maxHp;
  ACHIEVEMENTS.emit(damageEvent);
}

// --- Level Up ---

void GameActivity::checkLevelUp() {
  auto& p = GAME_STATE.player;

  while (p.charLevel < 50 && p.experience >= game::xpForLevel(p.charLevel + 1)) {
    p.charLevel++;

    game::GameEvent levelEvent{};
    levelEvent.type = game::GameEventType::LevelUp;
    levelEvent.newLevel = p.charLevel;
    ACHIEVEMENTS.emit(levelEvent);
    if (ACHIEVEMENTS.hasNewUnlock()) {
      gameRenderer.showNotification(NotificationKind::Achievement, ACHIEVEMENTS.consumeNewUnlockFlavor());
    }

    // Stat gains
    uint16_t hpGain = 4 + p.constitution / 4;
    uint16_t mpGain = 2 + p.intelligence / 5;
    p.maxHp += hpGain;
    p.maxMp += mpGain;
    p.hp = p.maxHp;  // Full heal on level up
    p.mp = p.maxMp;
    p.strength += 1;
    p.dexterity += 1;

    char msgBuf[96];
    const char* flavor = FLAVOR_TEXT.pick(game::FlavorCategory::LevelUp);
    snprintf(msgBuf, sizeof(msgBuf), "Welcome to level %u! %s", p.charLevel, flavor);
    GAME_STATE.addMessage(msgBuf);
    gameRenderer.showNotification(NotificationKind::LevelUp, msgBuf);
  }
}

// --- End Screen Data ---

void GameActivity::populateEndScreenData() {
  const auto& p = GAME_STATE.player;

  snprintf(endScreenData.cause, sizeof(endScreenData.cause), "%s", deathCause);
  endScreenData.floor = p.dungeonDepth;
  endScreenData.turns = p.turnCount;
  endScreenData.kills = p.kills;
  endScreenData.level = p.charLevel;

  endScreenData.unlockedCount = 0;
  for (uint8_t i = 0; i < static_cast<uint8_t>(game::AchievementId::Count); i++) {
    auto id = static_cast<game::AchievementId>(i);
    if (ACHIEVEMENTS.isUnlockedThisRun(id)) {
      endScreenData.unlockedIds[endScreenData.unlockedCount++] = id;
    }
  }
}

// --- Player Death ---

void GameActivity::handlePlayerDeath() {
  // Idempotent: handleAction()'s dead-player branch can still reach this after
  // the death screen is already up (e.g. a stray input before loop()'s dismiss
  // gating takes over on the very same frame) — don't redo the teardown twice.
  if (screenMode == GameScreenMode::Death) return;

  if (deathCause[0] == '\0') {
    snprintf(deathCause, sizeof(deathCause), "%s", "Unknown causes");
  }

  populateEndScreenData();

  // Delete save data — permadeath! Done immediately (not deferred to dismiss)
  // so an app crash/kill while the death screen is up can't leave a save file
  // for a character that's already dead.
  GameSave::deleteAll();
  GAME_STATE.deleteSaveFile();

  screenMode = GameScreenMode::Death;
  requestUpdate();
}

// --- Victory ---

void GameActivity::handleVictory() {
  // Idempotent, mirrors handlePlayerDeath()'s guard.
  if (screenMode == GameScreenMode::Victory) return;

  populateEndScreenData();

  // Clear save — the quest is complete
  GameSave::deleteAll();
  GAME_STATE.deleteSaveFile();

  screenMode = GameScreenMode::Victory;
  requestUpdate();
}

// --- Level Management ---

void GameActivity::loadOrGenerateLevel() {
  auto& p = GAME_STATE.player;

  // Always regenerate from seed (deterministic)
  auto result = DungeonGenerator::generate(p.gameSeed, p.dungeonDepth, tiles, monsters, levelItems);
  monsterCount = result.monsterCount;
  itemCount = result.itemCount;

  // Clear fog
  memset(fogOfWar, 0, sizeof(fogOfWar));
  memset(doorOpen, 0, sizeof(doorOpen));
  memset(visible, 0, sizeof(visible));

  // If we have saved state for this level, overlay it
  if (GameSave::hasLevel(p.dungeonDepth)) {
    // Load saved fog, door state, monsters, and items (overrides generated state)
    GameSave::loadLevel(p.dungeonDepth, fogOfWar, doorOpen, monsters, monsterCount, levelItems, itemCount);

    // DungeonGenerator::generate() above reset every door tile to DoorClosed;
    // re-open the ones the player had already opened before saving (Phase 7 req 6).
    for (int y = 0; y < game::MAP_HEIGHT; y++) {
      for (int x = 0; x < game::MAP_WIDTH; x++) {
        int idx = y * game::MAP_WIDTH + x;
        if (tiles[idx] == game::Tile::DoorClosed && game::fogIsExplored(doorOpen, x, y)) {
          tiles[idx] = game::Tile::DoorOpen;
        }
      }
    }
  } else {
    // First visit — place player at stairs up
    p.x = result.stairsUpX;
    p.y = result.stairsUpY;
  }
}

void GameActivity::saveCurrentLevel() {
  GameSave::saveLevel(GAME_STATE.player.dungeonDepth, fogOfWar, doorOpen, monsters, monsterCount, levelItems,
                      itemCount);
}

// --- Visibility ---

void GameActivity::computeVisibility() {
  auto& p = GAME_STATE.player;

  memset(visible, 0, sizeof(visible));

  // For each tile within FOV_RADIUS, check line of sight
  int startX = std::max(0, static_cast<int>(p.x) - FOV_RADIUS);
  int endX = std::min(game::MAP_WIDTH - 1, static_cast<int>(p.x) + FOV_RADIUS);
  int startY = std::max(0, static_cast<int>(p.y) - FOV_RADIUS);
  int endY = std::min(game::MAP_HEIGHT - 1, static_cast<int>(p.y) + FOV_RADIUS);

  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      int dx = x - p.x;
      int dy = y - p.y;
      if (dx * dx + dy * dy > FOV_RADIUS * FOV_RADIUS) continue;

      if (hasLineOfSight(tiles, p.x, p.y, x, y)) {
        visible[y * game::MAP_WIDTH + x] = true;
        game::fogSetExplored(fogOfWar, x, y);
      }
    }
  }
}
