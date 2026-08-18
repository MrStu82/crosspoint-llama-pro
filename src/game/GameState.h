#pragma once

#include <string>

#include "GameTypes.h"
#include "SaveValidity.h"

class GameState {
  static GameState instance;

 public:
  game::Player player;
  game::Item inventory[game::MAX_INVENTORY];
  uint8_t inventoryCount = 0;

  // Message log (ring buffer of recent messages)
  std::string messages[game::MAX_MESSAGES];
  uint8_t messageCount = 0;
  uint8_t messageHead = 0;  // Index of oldest message in ring buffer

  ~GameState() = default;

  static GameState& getInstance() { return instance; }

  // Reset to new game defaults
  void newGame(uint32_t seed);

  // Add a message to the log
  void addMessage(const char* msg);

  // Draw from the persistent combat RNG stream (player.combatRngState), advancing it in place.
  // Never construct a local game::Rng for a combat/AI roll — always go through these so the
  // stream is genuinely continuous across the whole run and correctly serialises with the save.
  uint32_t rollRange(uint32_t max);
  int rollRangeInclusive(int min, int max);

  // Get the Nth most recent message (0 = most recent)
  const std::string& getMessage(int recencyIndex) const;

  // Persistence
  bool saveToFile() const;
  bool loadFromFile();
  bool hasSaveFile() const;
  void deleteSaveFile() const;

  // Read-only validity check for save.bin -- does it exist, and if so, is it
  // loadable? Internally shares loadFromFile()'s exact parse/validate path
  // (see GameState.cpp), so this can never drift from what loadFromFile()
  // actually accepts or rejects. Used by both the load-boundary notification
  // and the Save Data Audit menu scan.
  static SaveValidity validateSaveFile();
};

#define GAME_STATE GameState::getInstance()

namespace game {

// Total attack/defense bonus from equipped weapons/armor/shields plus the
// active sponsor modifier. Shared by GameActivity's combat math and the
// character/inventory screens' gear-effect displays -- one computation, read
// everywhere it's needed, never mutated into a base stat (see the sponsor
// comment on Player::activeSponsorId in GameTypes.h).
inline int equippedAttackBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(ItemFlag::Equipped)) {
      for (int d = 0; d < ITEM_DEF_COUNT; d++) {
        if (ITEM_DEFS[d].type == item.type && ITEM_DEFS[d].subtype == item.subtype) {
          bonus += ITEM_DEFS[d].attack + item.enchantment;
          break;
        }
      }
    }
  }
  bonus += sponsorAttackModifier(GAME_STATE.player.activeSponsorId);
  bonus += buffAttackModifier(GAME_STATE.player);
  return bonus;
}

inline int equippedDefenseBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(ItemFlag::Equipped)) {
      for (int d = 0; d < ITEM_DEF_COUNT; d++) {
        if (ITEM_DEFS[d].type == item.type && ITEM_DEFS[d].subtype == item.subtype) {
          bonus += ITEM_DEFS[d].defense + item.enchantment;
          break;
        }
      }
    }
  }
  bonus += sponsorDefenseModifier(GAME_STATE.player.activeSponsorId);
  bonus += buffDefenseModifier(GAME_STATE.player);
  return bonus;
}

}  // namespace game
