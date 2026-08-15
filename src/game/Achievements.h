#pragma once

#include <cstdint>

namespace game {

enum class AchievementId : uint8_t {
  Ding = 0,
  ThatllBuffOut = 1,
  AudienceParticipation = 2,
  EscalationOfForce = 3,
  Count = 4,
};

enum class GameEventType : uint8_t {
  MonsterKilled,
  PlayerDamaged,
  PlayerDied,
  FloorChanged,
  ItemUsed,
  LevelUp,
};

struct GameEvent {
  GameEventType type;
  uint16_t damage = 0;        // MonsterKilled: damage dealt on the killing blow.
  uint16_t monsterMaxHp = 0;  // MonsterKilled/PlayerDied: killer's/victim's MonsterDef::baseHp.
  uint16_t monsterAttack = 0; // PlayerDied: MonsterDef::attack of the killer.
  uint16_t hpAfter = 0;       // PlayerDamaged: player's hp after the hit.
  uint16_t maxHp = 0;         // PlayerDamaged: player's maxHp at the time.
  uint8_t newLevel = 0;       // LevelUp: new character level.
};

}  // namespace game
