#pragma once

#include <cstdint>

#include "GameTypes.h"

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
  SponsoredContent = 9,
  // -- Depth --
  FirstSteps = 10,
  DownWeGo = 11,
  GettingComfortable = 12,
  DeepDelver = 13,
  PressureTolerance = 14,
  AbyssWalker = 15,
  StructurallyUnsound = 16,
  ExpressDescent = 17,
  DungeonSovereign = 18,
  // -- Combat --
  FirstBlood = 19,
  Ratcatcher = 20,
  Exterminator = 21,
  Beastbane = 22,
  Overkill = 23,
  GiantKiller = 24,
  CleanSweep = 25,
  Untouched = 26,
  PacifistRun = 27,
  // -- Survival --
  OneHitPoint = 28,
  TheUnkilled = 29,
  Veteran = 30,
  Seasoned = 31,
  LongHaul = 32,
  Attrition = 33,
  BackFromTheBrink = 34,
  DiedAnyway = 35,
  // -- Exploration --
  Cartographer = 36,
  Thorough = 37,
  Obsessive = 38,
  Shortcut = 39,
  ScenicRoute = 40,
  Wanderer = 41,
  Pathfinder = 42,
  // -- Loot --
  FindersKeepers = 43,
  Magpie = 44,
  TheWealthy = 45,
  ObsceneWealth = 46,
  // -- Secrets --
  Completionist = 47,
  // -- Demo set: proves each condition-table type before the real pool is authored --
  // CounterCompare
  IronStomach = 48,       // Hunger >= threshold sustained (silly counter demo)
  StepCounter = 49,       // TilesWalked >= threshold (already-real-field counter demo)
  // EventFieldMatch
  OneShot = 50,           // MonsterKilled with damage >= huge single-hit value
  NickOfTime = 51,        // PlayerDamaged with hpAfter == 1 field match
  // EventCount
  SerialKiller = 52,      // MonsterKilled count >= N within a run
  Klepto = 53,            // ItemPickedUp count >= N within a run
  // NoEventInWindow
  Ghost = 54,             // no PlayerDamaged event for whole run
  Untroubled = 55,        // no PlayerDamaged event for a single floor
  // ItemSpecific
  PotionChugger = 56,     // ItemUsed with itemType == Potion, count threshold
  ScrollHoarder = 57,     // ItemPickedUp with itemType == Scroll, count threshold
  // Compound (AND of two condition rows)
  GreedyAndFast = 58,     // TheWealthy-style AND SpeedRunner-style combo
  DeepAndDeadly = 59,     // DeepDiver-style AND Exterminator-style combo
  Count = 60,
};

// Bounds-safe lookup: AchievementId now runs past ACHIEVEMENT_DEF_COUNT (the
// data-driven pool from IronStomach/48 onward has no ACHIEVEMENT_DEFS entry
// yet -- that table lands with the real reward work, amendment 2). Indexing
// ACHIEVEMENT_DEFS directly by id would read out of bounds for any of those
// ids the moment one actually unlocks. Callers that need a def for a
// possibly-undefined id should go through this instead of the raw array.
inline const AchievementDef& achievementDef(AchievementId id) {
  static constexpr AchievementDef kFallback{"", "", AchievementReward::None, 0};
  uint8_t idx = static_cast<uint8_t>(id);
  if (idx >= ACHIEVEMENT_DEF_COUNT) return kFallback;
  return ACHIEVEMENT_DEFS[idx];
}

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
    case AchievementId::SponsoredContent: return "Sponsored Content";
    case AchievementId::FirstSteps: return "First Steps";
    case AchievementId::DownWeGo: return "Down We Go";
    case AchievementId::GettingComfortable: return "Getting Comfortable";
    case AchievementId::DeepDelver: return "Deep Delver";
    case AchievementId::PressureTolerance: return "Pressure Tolerance";
    case AchievementId::AbyssWalker: return "Abyss-Walker";
    case AchievementId::StructurallyUnsound: return "Structurally Unsound";
    case AchievementId::ExpressDescent: return "Express Descent";
    case AchievementId::DungeonSovereign: return "Dungeon Sovereign";
    case AchievementId::FirstBlood: return "First Blood";
    case AchievementId::Ratcatcher: return "Ratcatcher";
    case AchievementId::Exterminator: return "Exterminator";
    case AchievementId::Beastbane: return "Beastbane";
    case AchievementId::Overkill: return "Overkill";
    case AchievementId::GiantKiller: return "Giant-Killer";
    case AchievementId::CleanSweep: return "Clean Sweep";
    case AchievementId::Untouched: return "Untouched";
    case AchievementId::PacifistRun: return "Pacifist Run";
    case AchievementId::OneHitPoint: return "One Hit Point";
    case AchievementId::TheUnkilled: return "The Unkilled";
    case AchievementId::Veteran: return "Veteran";
    case AchievementId::Seasoned: return "Seasoned";
    case AchievementId::LongHaul: return "Long Haul";
    case AchievementId::Attrition: return "Attrition";
    case AchievementId::BackFromTheBrink: return "Back From The Brink";
    case AchievementId::DiedAnyway: return "Died Anyway";
    case AchievementId::Cartographer: return "Cartographer";
    case AchievementId::Thorough: return "Thorough";
    case AchievementId::Obsessive: return "Obsessive";
    case AchievementId::Shortcut: return "Shortcut";
    case AchievementId::ScenicRoute: return "Scenic Route";
    case AchievementId::Wanderer: return "Wanderer";
    case AchievementId::Pathfinder: return "Pathfinder";
    case AchievementId::FindersKeepers: return "Finders Keepers";
    case AchievementId::Magpie: return "Magpie";
    case AchievementId::TheWealthy: return "The Wealthy";
    case AchievementId::ObsceneWealth: return "Obscene Wealth";
    case AchievementId::Completionist: return "Completionist";
    case AchievementId::IronStomach: return "Iron Stomach";
    case AchievementId::StepCounter: return "Step Counter";
    case AchievementId::OneShot: return "One Shot";
    case AchievementId::NickOfTime: return "Nick of Time";
    case AchievementId::SerialKiller: return "Serial Killer";
    case AchievementId::Klepto: return "Klepto";
    case AchievementId::Ghost: return "Ghost";
    case AchievementId::Untroubled: return "Untroubled";
    case AchievementId::PotionChugger: return "Potion Chugger";
    case AchievementId::ScrollHoarder: return "Scroll Hoarder";
    case AchievementId::GreedyAndFast: return "Greedy and Fast";
    case AchievementId::DeepAndDeadly: return "Deep and Deadly";
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
  LootBoxOpened,
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
  uint16_t hpBeforeHit = 0;   // MonsterKilled: killed monster's hp immediately before the killing blow.
  bool cleanSweep = false;    // MonsterKilled (synthetic follow-up event): true if that kill cleared the floor.
  bool floorFullyExplored = false; // FloorChanged: true if every walkable tile on the departed floor was seen.
  uint8_t monsterMinDepth = 0; // MonsterKilled: killed monster's MonsterDef::minDepth (for Giant-Killer).
  ItemType itemType = ItemType::ItemTypeCount; // ItemUsed/ItemPickedUp/ItemThrown: type of the item involved (ItemTypeCount = none).
};

}  // namespace game
