#pragma once

#include <cstdint>

namespace game {

enum class AchievementId : uint8_t {
  Ding = 0,
  ThatllBuffOut = 1,
  AudienceParticipation = 2,
  EscalationOfForce = 3,
  PackRat = 4,
  SpeedRunner = 5,
  DeepDiver = 6,
  MaxedOut = 7,
  PercussiveMaintenance = 8,
  Count = 9,
};

// Short display name for the end-of-run screen (Phase 7 req 2/3). Kept separate
// from the flavor-text unlock messages in AchievementBus::emit(), which are
// chat-log lines, not a fixed-width UI label.
inline const char* achievementShortName(AchievementId id) {
  switch (id) {
    case AchievementId::Ding: return "Ding!";
    case AchievementId::ThatllBuffOut: return "That'll Buff Out";
    case AchievementId::AudienceParticipation: return "Audience Participation";
    case AchievementId::EscalationOfForce: return "Escalation of Force";
    case AchievementId::PackRat: return "Pack Rat";
    case AchievementId::SpeedRunner: return "Speed Runner";
    case AchievementId::DeepDiver: return "Deep Diver";
    case AchievementId::MaxedOut: return "Maxed Out";
    case AchievementId::PercussiveMaintenance: return "Percussive Maintenance";
    default: return "";
  }
}

// Vague category teaser for a still-locked achievement (Phase 9 work item 4 --
// "locked redacted with a hint"). Never gives away the exact trigger condition,
// only the rough flavor of it -- the real name/flavorText stays hidden until
// AchievementBus::unlock() actually fires (see req 5: a locked one is never named).
inline const char* achievementHint(AchievementId id) {
  switch (id) {
    case AchievementId::Ding: return "Grow stronger.";
    case AchievementId::ThatllBuffOut: return "Survive the unsurvivable.";
    case AchievementId::AudienceParticipation: return "Die to something weak.";
    case AchievementId::EscalationOfForce: return "Overkill a monster.";
    case AchievementId::PackRat: return "Fill your pockets.";
    case AchievementId::SpeedRunner: return "Move fast.";
    case AchievementId::DeepDiver: return "Go deep.";
    case AchievementId::MaxedOut: return "Reach your peak.";
    case AchievementId::PercussiveMaintenance: return "Solve a problem with a thrown object.";
    default: return "";
  }
}

enum class GameEventType : uint8_t {
  MonsterKilled,
  PlayerDamaged,
  PlayerDied,
  FloorChanged,
  ItemUsed,
  ItemPickedUp,
  LevelUp,
  ItemThrown,
};

struct GameEvent {
  GameEventType type;
  uint16_t damage = 0;        // MonsterKilled: damage dealt on the killing blow.
  uint16_t monsterMaxHp = 0;  // MonsterKilled/PlayerDied: killer's/victim's MonsterDef::baseHp.
  uint16_t monsterAttack = 0; // PlayerDied: MonsterDef::attack of the killer.
  uint16_t hpAfter = 0;       // PlayerDamaged: player's hp after the hit.
  uint16_t maxHp = 0;         // PlayerDamaged: player's maxHp at the time.
  uint8_t newLevel = 0;       // LevelUp: new character level.
  bool killedMonster = false; // ItemThrown: true if the thrown item's hit killed its target.
};

}  // namespace game
