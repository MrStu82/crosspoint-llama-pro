#pragma once

#include <cstdio>
#include <cstring>

#include "GameState.h"
#include "GameTypes.h"

// Companion pet logic (Job Phase 3). Kept as free functions taking/returning
// game:: types, same shape as equippedAttackBonus()/equippedDefenseBonus()
// in GameState.h, so this stays linkable and testable outside the firmware
// build (host gtest) exactly like the rest of game/*.h.
namespace game {

// Rolls a brand-new pet: species, name, and small per-instance stat spread,
// all drawn from the persistent combat RNG stream (GAME_STATE.rollRange /
// rollRangeInclusive) so a save+reload reproduces the exact same pet --
// never a locally constructed game::Rng, which would not survive a reload.
inline void rollNewPet(Pet& pet) {
  pet.active = true;
  pet.speciesId = static_cast<uint8_t>(GAME_STATE.rollRange(PET_SPECIES_COUNT));
  const char* rolledName = PET_NAME_POOL[GAME_STATE.rollRange(PET_NAME_COUNT)];
  snprintf(pet.name, sizeof(pet.name), "%s", rolledName);
  // Small per-instance spread so littermates of the same species still differ.
  pet.hpBase = static_cast<uint8_t>(GAME_STATE.rollRangeInclusive(8, 14));
  pet.attackBase = static_cast<uint8_t>(GAME_STATE.rollRangeInclusive(1, 3));
  pet.defenseBase = static_cast<uint8_t>(GAME_STATE.rollRangeInclusive(0, 2));
  pet.hasGear = false;
  pet.gear = Item{};
}

// The pet levels with the player rather than tracking its own XP/level --
// no separate storage, just reads player.charLevel directly at the point of
// use (same point-of-use pattern as the buff/sponsor modifiers above).
inline uint16_t petLevel() { return GAME_STATE.player.charLevel; }

inline const ItemDef* petFindItemDef(const Item& item) {
  for (int d = 0; d < ITEM_DEF_COUNT; d++) {
    if (ITEM_DEFS[d].type == item.type && ITEM_DEFS[d].subtype == item.subtype) {
      return &ITEM_DEFS[d];
    }
  }
  return nullptr;
}

// Combined attack+defense contribution of a candidate gear item. The pet has
// exactly one gear slot (not per-type slots like the player's equipment
// list), so a single scalar lets petForage() compare "is this find better
// than what I'm already carrying" regardless of whether either item is a
// weapon or armor/shield.
inline int petGearPower(const Item& item) {
  const ItemDef* def = petFindItemDef(item);
  if (def == nullptr) return 0;
  return def->attack + def->defense + item.enchantment;
}

inline uint16_t petMaxHp(const Pet& pet) {
  return static_cast<uint16_t>(pet.hpBase + (petLevel() - 1) * 2);
}

inline int petAttack(const Pet& pet) {
  int gearBonus = 0;
  if (pet.hasGear) {
    const ItemDef* def = petFindItemDef(pet.gear);
    if (def != nullptr) gearBonus = def->attack + pet.gear.enchantment;
  }
  return pet.attackBase + (petLevel() - 1) + gearBonus;
}

inline int petDefense(const Pet& pet) {
  int gearBonus = 0;
  if (pet.hasGear) {
    const ItemDef* def = petFindItemDef(pet.gear);
    if (def != nullptr) gearBonus = def->defense + pet.gear.enchantment;
  }
  return pet.defenseBase + gearBonus;
}

// Consumes one freshly-foraged PetLootStream item. The pet carries no
// inventory of its own -- only weapon/armor/shield finds are even candidates,
// and only kept if strictly stronger (by petGearPower) than whatever gear it
// already has. Everything else (potions, scrolls, food, gold, quest items)
// is simply not useful to a companion with one gear slot and is dropped on
// the spot; nothing accumulates unread in the stream. Called once per new
// PetLootStream entry, right where GameActivity::dropCorpseLoot() appends it.
inline void petForage(Pet& pet, const Item& found) {
  if (!pet.active) return;
  auto type = static_cast<ItemType>(found.type);
  if (type != ItemType::Weapon && type != ItemType::Armor && type != ItemType::Shield) return;

  const int newPower = petGearPower(found);
  const int currentPower = pet.hasGear ? petGearPower(pet.gear) : -1;
  if (newPower > currentPower) {
    pet.gear = found;
    pet.hasGear = true;
  }
}

}  // namespace game
