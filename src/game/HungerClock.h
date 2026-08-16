#pragma once

#include <cstdint>

#include "GameTypes.h"

namespace game {

// Outcome of a single hunger tick, over plain hunger/hp state -- lets the
// caller (GameActivity::processMonsterTurns()) pick the right message/return
// without the tick logic itself touching GAME_STATE.addMessage() or any
// rendering dependency. Mirrors the original inline block exactly; no new
// state, no behaviour change.
enum class HungerTickOutcome {
  Ticked,               // hunger incremented, no threshold crossed
  HitHungryThreshold,   // hunger incremented, crossed HUNGER_HUNGRY_THRESHOLD
  HitStarvingThreshold, // hunger incremented, crossed HUNGER_STARVING_THRESHOLD
  TookStarveDamage,     // hunger already capped; starvation damage applied, hp > 0
  Died,                 // hunger already capped; starvation damage applied, hp == 0
  NoOp,                 // hunger already capped and hp already 0 -- nothing to do
};

// Advances the hunger clock by exactly one turn. Free of GameActivity/GAME_STATE
// so a host harness can link it directly instead of re-implementing the formula.
inline HungerTickOutcome tickHunger(uint16_t& hunger, uint16_t& hp) {
  if (hunger < HUNGER_MAX) {
    hunger++;
    if (hunger == HUNGER_HUNGRY_THRESHOLD) return HungerTickOutcome::HitHungryThreshold;
    if (hunger == HUNGER_STARVING_THRESHOLD) return HungerTickOutcome::HitStarvingThreshold;
    return HungerTickOutcome::Ticked;
  }
  if (hp > 0) {
    uint16_t dmg = HUNGER_STARVE_DAMAGE;
    hp = (dmg >= hp) ? 0 : static_cast<uint16_t>(hp - dmg);
    return (hp == 0) ? HungerTickOutcome::Died : HungerTickOutcome::TookStarveDamage;
  }
  return HungerTickOutcome::NoOp;
}

// Eating (GameMenuActivity's Food case): fully relieves hunger. Item use never
// advances turnCount, so this is always instant and safe regardless of current
// hp/hunger -- the guarantee that makes the hunger clock escapable from the
// worst reachable state (max hunger, 1 hp) as long as one food item exists.
inline void eatAndResetHunger(uint16_t& hunger) { hunger = 0; }

}  // namespace game
