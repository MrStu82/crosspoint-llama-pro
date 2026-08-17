#pragma once

#include <cstdint>

#include "GameTypes.h"

namespace game {

// 60-entry replacement of the original 10 (Phase 12). Index == array index
// into game::ACHIEVEMENT_DEFS (GameTypes.h) == bit position persisted in
// AchievementBus's unlock bitmask. APPEND-ONLY -- never reorder or delete an
// entry, only add new ones before Count.
enum class AchievementId : uint8_t {
  // ---- Depth (0-9) ----
  FirstSteps = 0,
  DownWeGo = 1,
  GettingComfortable = 2,
  NoDaylight = 3,
  DeepDelver = 4,
  PressureTolerance = 5,
  AbyssWalker = 6,
  StructurallyUnsound = 7,
  ExpressDescent = 8,
  DungeonSovereign = 9,
  // ---- Combat (10-21) ----
  FirstBlood = 10,
  Ratcatcher = 11,
  Exterminator = 12,
  Beastbane = 13,
  Overkill = 14,
  GiantKiller = 15,
  CleanSweep = 16,
  Untouched = 17,
  DoubleTap = 18,
  Surrounded = 19,
  CriticalThinking = 20,
  PacifistRun = 21,
  // ---- Survival (22-31) ----
  OneHitPoint = 22,
  CloseShave = 23,
  TheUnkilled = 24,
  Veteran = 25,
  Seasoned = 26,
  Ascendant = 27,
  LongHaul = 28,
  Attrition = 29,
  BackFromTheBrink = 30,
  DiedAnyway = 31,
  // ---- Exploration (32-41) ----
  Cartographer = 32,
  Thorough = 33,
  Obsessive = 34,
  DeadEnd = 35,
  Shortcut = 36,
  ScenicRoute = 37,
  Homebody = 38,
  Cornered = 39,
  Wanderer = 40,
  Pathfinder = 41,
  // ---- Loot and economy (42-51) ----
  FindersKeepers = 42,
  Magpie = 43,
  Hoarder = 44,
  TheWealthy = 45,
  ObsceneWealth = 46,
  TheFrugal = 47,
  Blademaster = 48,
  WellDressed = 49,
  TravellingLight = 50,
  WasteNot = 51,
  // ---- Curiosities and secrets (52-59) ----
  ReadTheManual = 52,
  TalkingToYourself = 53,
  Archivist = 54,
  TheCurious = 55,
  TheLucky = 56,
  SponsoredContent = 57,
  BrandLoyalty = 58,
  Completionist = 59,
  Count = 60,
};

// Short display name for the end-of-run/achievements-menu screen. Just the
// def's own name -- kept as a function (not a raw array index at call sites)
// so callers don't need to know ACHIEVEMENT_DEFS's layout.
inline const char* achievementShortName(AchievementId id) {
  uint8_t i = static_cast<uint8_t>(id);
  if (i >= ACHIEVEMENT_DEF_COUNT) return "";
  return ACHIEVEMENT_DEFS[i].name;
}

// Vague category teaser for a still-locked achievement. Deliberately NEVER
// returns ACHIEVEMENT_DEFS[i].description (the real unlock condition) or
// .name -- GameMenuActivity.cpp renders locked rows as
// "<Locked label> -- <hint>", so this only needs a short phrase in the same
// rough category as the real condition, never the condition itself.
inline const char* achievementHint(AchievementId id) {
  switch (id) {
    // ---- Depth ----
    case AchievementId::FirstSteps: return "Begin.";
    case AchievementId::DownWeGo: return "Go down a level.";
    case AchievementId::GettingComfortable: return "Go deeper still.";
    case AchievementId::NoDaylight: return "Go deeper yet.";
    case AchievementId::DeepDelver: return "Keep descending.";
    case AchievementId::PressureTolerance: return "Descend further.";
    case AchievementId::AbyssWalker: return "Descend further still.";
    case AchievementId::StructurallyUnsound: return "Reach the very bottom.";
    case AchievementId::ExpressDescent: return "Descend quickly.";
    case AchievementId::DungeonSovereign: return "Reach the deepest floor.";
    // ---- Combat ----
    case AchievementId::FirstBlood: return "Fight something.";
    case AchievementId::Ratcatcher: return "Kill a few things.";
    case AchievementId::Exterminator: return "Kill many things.";
    case AchievementId::Beastbane: return "Kill a great many things.";
    case AchievementId::Overkill: return "Hit too hard.";
    case AchievementId::GiantKiller: return "Punch above your weight.";
    case AchievementId::CleanSweep: return "Clear a floor of threats.";
    case AchievementId::Untouched: return "Fight without being hit.";
    case AchievementId::DoubleTap: return "Kill quickly, twice.";
    case AchievementId::Surrounded: return "Survive being outnumbered.";
    case AchievementId::CriticalThinking: return "Land a lucky hit.";
    case AchievementId::PacifistRun: return "Descend without fighting.";
    // ---- Survival ----
    case AchievementId::OneHitPoint: return "Live on the edge.";
    case AchievementId::CloseShave: return "Nearly die and don't.";
    case AchievementId::TheUnkilled: return "Grow strong without dying.";
    case AchievementId::Veteran: return "Grow stronger.";
    case AchievementId::Seasoned: return "Grow stronger still.";
    case AchievementId::Ascendant: return "Grow stronger yet.";
    case AchievementId::LongHaul: return "Endure a long run.";
    case AchievementId::Attrition: return "Endure a very long run.";
    case AchievementId::BackFromTheBrink: return "Recover from near death.";
    case AchievementId::DiedAnyway: return "Die.";
    // ---- Exploration ----
    case AchievementId::Cartographer: return "Explore fully.";
    case AchievementId::Thorough: return "Explore fully, repeatedly.";
    case AchievementId::Obsessive: return "Explore fully, obsessively.";
    case AchievementId::DeadEnd: return "Find nothing.";
    case AchievementId::Shortcut: return "Find the way out fast.";
    case AchievementId::ScenicRoute: return "Take your time on a floor.";
    case AchievementId::Homebody: return "Go somewhere familiar.";
    case AchievementId::Cornered: return "Explore before fighting.";
    case AchievementId::Wanderer: return "Walk a long way.";
    case AchievementId::Pathfinder: return "Walk a very long way.";
    // ---- Loot and economy ----
    case AchievementId::FindersKeepers: return "Pick something up.";
    case AchievementId::Magpie: return "Collect a lot of things.";
    case AchievementId::Hoarder: return "Fill your pockets.";
    case AchievementId::TheWealthy: return "Accumulate gold.";
    case AchievementId::ObsceneWealth: return "Accumulate a lot of gold.";
    case AchievementId::TheFrugal: return "Descend without spending.";
    case AchievementId::Blademaster: return "Wield the finest steel.";
    case AchievementId::WellDressed: return "Dress for the occasion.";
    case AchievementId::TravellingLight: return "Carry almost nothing.";
    case AchievementId::WasteNot: return "Use something right away.";
    // ---- Curiosities and secrets ----
    case AchievementId::ReadTheManual: return "Check the help screen.";
    case AchievementId::TalkingToYourself: return "Say too much at once.";
    case AchievementId::Archivist: return "Uncover hidden lore.";
    case AchievementId::TheCurious: return "Look before you leap.";
    case AchievementId::TheLucky: return "Cheat death.";
    case AchievementId::SponsoredContent: return "Attract attention.";
    case AchievementId::BrandLoyalty: return "Stay loyal.";
    case AchievementId::Completionist: return "Do almost everything else.";
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
  HelpScreenOpened,
  MonsterExamined,
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
  uint16_t hpBeforeHit = 0;   // MonsterKilled: monster's hp immediately before the killing
                              // blow (i.e. the hp the killing damage was dealt against) --
                              // used by Overkill, distinct from monsterMaxHp.
  bool nearLethalSurvival = false;  // PlayerDamaged: true if this hit's damage was within
                                     // 10% of the player's hp-before-hit and the player is
                                     // still alive -- used by The Lucky.
  uint8_t adjacentHostileCount = 0;  // PlayerDamaged: number of hostile monsters adjacent to
                                      // the player (including the attacker) at the moment this
                                      // hit resolved -- used by Surrounded.
  bool wasCriticalHit = false;       // MonsterKilled/PlayerDamaged: true if this hit's damage
                                      // roll landed in the top variance band for its attack
                                      // (see GameActivity.cpp call sites) -- used by Critical
                                      // Thinking. Distinct from wasCriticalThisFloor in
                                      // AchievementBus, which tracks "dropped below 10% HP".
  bool cleanSweep = false;           // MonsterKilled: true if this kill left no other living
                                      // monster on the current floor -- used by Clean Sweep.
                                      // Sent as a second, minimal MonsterKilled event right
                                      // after the real kill event so the two concerns (kill
                                      // bookkeeping vs. floor-cleared check) stay independent.
  uint8_t exploredPctOfFloorLeft = 0;  // FloorChanged: percentage (0-100) of the floor just
                                       // left that was ever marked explored in fogOfWar,
                                       // computed by GameActivity from its own tiles/fogOfWar
                                       // arrays before they're overwritten by the new floor's
                                       // load/generate -- used by Cartographer/Thorough/
                                       // Obsessive/Dead End.
  bool revisitedPriorFloor = false;   // FloorChanged: true if the floor being entered has a
                                       // saved level file already on disk (i.e. the player has
                                       // been here before this run, most commonly via stairs
                                       // up) -- used by Homebody.
};

}  // namespace game
