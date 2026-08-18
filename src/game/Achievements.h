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
  // -- Depth bucket (Milestone 1): every remaining single-floor depth
  // threshold (60-76), then depth-gated compounds against the demo pool's
  // normal condition rows (77-89). CounterCompare + Compound only -- see
  // AchievementConditions.h for the condition rows themselves. --
  Depth3 = 60,
  Depth4 = 61,
  Depth6 = 62,
  Depth7 = 63,
  Depth8 = 64,
  Depth9 = 65,
  Depth11 = 66,
  Depth12 = 67,
  Depth13 = 68,
  Depth14 = 69,
  Depth16 = 70,
  Depth17 = 71,
  Depth18 = 72,
  Depth19 = 73,
  Depth21 = 74,
  Depth23 = 75,
  Depth24 = 76,
  TreaderOfShallows = 77,    // Depth3 AND StepCounter
  ButcherOfSix = 78,         // Depth6 AND SerialKiller
  UnscathedEighth = 79,      // Depth8 AND Ghost
  SereneNinth = 80,          // Depth9 AND Untroubled
  AlchemistOfEleven = 81,    // Depth11 AND PotionChugger
  ScribeOfTwelve = 82,       // Depth12 AND ScrollHoarder
  HoarderOfThirteen = 83,    // Depth13 AND Klepto
  WandererOfFourteen = 84,   // Depth14 AND StepCounter
  ReaperOfSixteen = 85,      // Depth16 AND SerialKiller
  PhantomOfSeventeen = 86,   // Depth17 AND Ghost
  UnbrokenNineteen = 87,     // Depth19 AND Untroubled
  BrewmasterOfTwentyOne = 88, // Depth21 AND PotionChugger
  LoremasterOfTwentyThree = 89, // Depth23 AND ScrollHoarder
  // -- Combat bucket (Milestone 2, ids 90-124): kill-count tiers, single-hit
  // damage tiers, hits-survived tiers, out-levelled-kill tiers, thrown-weapon
  // tiers, and pacifist-descent compounds. See AchievementConditions.h for
  // the condition rows (29-63). --
  Bloodletter = 90,
  Cutthroat = 91,
  HuntersTally = 92,
  BodyCount = 93,
  Warpath = 94,
  Slaughterhouse = 95,
  KillStreak = 96,
  Merciless = 97,
  GenocideRun = 98,
  ApexPredator = 99,
  FirstCut = 100,
  HeavyHand = 101,
  BoneBreaker = 102,
  CrushingBlow = 103,
  Devastator = 104,
  AnnihilatingStrike = 105,
  WorldEnder = 106,
  Cataclysm = 107,
  BruisedNotBroken = 108,
  PunchingBag = 109,
  GluttonForPunishment = 110,
  Besieged = 111,
  LastOneStanding = 112,
  Overmatched = 113,
  Outgunned = 114,
  InOverYourHead = 115,
  DavidAndGoliath = 116,
  QuickDraw = 117,
  RangedSpecialist = 118,
  MasterMarksman = 119,
  Bloodless = 120,
  PacifistToSix = 121,
  PacifistToEleven = 122,
  PacifistToSixteen = 123,
  PeacefulSovereign = 124,
  Count = 125,
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
    case AchievementId::Depth3: return "Footing";
    case AchievementId::Depth4: return "Four Floors Down";
    case AchievementId::Depth6: return "Sixth Sense";
    case AchievementId::Depth7: return "Lucky Seven";
    case AchievementId::Depth8: return "Eight Below";
    case AchievementId::Depth9: return "Ninth Circle";
    case AchievementId::Depth11: return "Eleven and Counting";
    case AchievementId::Depth12: return "Dozen Deep";
    case AchievementId::Depth13: return "Unlucky Thirteen";
    case AchievementId::Depth14: return "Fourteen Fathoms";
    case AchievementId::Depth16: return "Sixteen Strides";
    case AchievementId::Depth17: return "Seventeen Sunken";
    case AchievementId::Depth18: return "Eighteen Echoes";
    case AchievementId::Depth19: return "Nineteen Nadir";
    case AchievementId::Depth21: return "Twenty-One Undertow";
    case AchievementId::Depth23: return "Twenty-Three Threshold";
    case AchievementId::Depth24: return "Twenty-Four Frontier";
    case AchievementId::TreaderOfShallows: return "Treader of Shallows";
    case AchievementId::ButcherOfSix: return "Butcher of Six";
    case AchievementId::UnscathedEighth: return "Unscathed Eighth";
    case AchievementId::SereneNinth: return "Serene Ninth";
    case AchievementId::AlchemistOfEleven: return "Alchemist of Eleven";
    case AchievementId::ScribeOfTwelve: return "Scribe of Twelve";
    case AchievementId::HoarderOfThirteen: return "Hoarder of Thirteen";
    case AchievementId::WandererOfFourteen: return "Wanderer of Fourteen";
    case AchievementId::ReaperOfSixteen: return "Reaper of Sixteen";
    case AchievementId::PhantomOfSeventeen: return "Phantom of Seventeen";
    case AchievementId::UnbrokenNineteen: return "Unbroken Nineteen";
    case AchievementId::BrewmasterOfTwentyOne: return "Brewmaster of Twenty-One";
    case AchievementId::LoremasterOfTwentyThree: return "Loremaster of Twenty-Three";
    case AchievementId::Bloodletter: return "Bloodletter";
    case AchievementId::Cutthroat: return "Cutthroat";
    case AchievementId::HuntersTally: return "Hunter's Tally";
    case AchievementId::BodyCount: return "Body Count";
    case AchievementId::Warpath: return "Warpath";
    case AchievementId::Slaughterhouse: return "Slaughterhouse";
    case AchievementId::KillStreak: return "Kill Streak";
    case AchievementId::Merciless: return "Merciless";
    case AchievementId::GenocideRun: return "Genocide Run";
    case AchievementId::ApexPredator: return "Apex Predator";
    case AchievementId::FirstCut: return "First Cut";
    case AchievementId::HeavyHand: return "Heavy Hand";
    case AchievementId::BoneBreaker: return "Bone Breaker";
    case AchievementId::CrushingBlow: return "Crushing Blow";
    case AchievementId::Devastator: return "Devastator";
    case AchievementId::AnnihilatingStrike: return "Annihilating Strike";
    case AchievementId::WorldEnder: return "World Ender";
    case AchievementId::Cataclysm: return "Cataclysm";
    case AchievementId::BruisedNotBroken: return "Bruised Not Broken";
    case AchievementId::PunchingBag: return "Punching Bag";
    case AchievementId::GluttonForPunishment: return "Glutton for Punishment";
    case AchievementId::Besieged: return "Besieged";
    case AchievementId::LastOneStanding: return "Last One Standing";
    case AchievementId::Overmatched: return "Overmatched";
    case AchievementId::Outgunned: return "Outgunned";
    case AchievementId::InOverYourHead: return "In Over Your Head";
    case AchievementId::DavidAndGoliath: return "David and Goliath";
    case AchievementId::QuickDraw: return "Quick Draw";
    case AchievementId::RangedSpecialist: return "Ranged Specialist";
    case AchievementId::MasterMarksman: return "Master Marksman";
    case AchievementId::Bloodless: return "Bloodless";
    case AchievementId::PacifistToSix: return "Pacifist to Six";
    case AchievementId::PacifistToEleven: return "Pacifist to Eleven";
    case AchievementId::PacifistToSixteen: return "Pacifist to Sixteen";
    case AchievementId::PeacefulSovereign: return "Peaceful Sovereign";
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
