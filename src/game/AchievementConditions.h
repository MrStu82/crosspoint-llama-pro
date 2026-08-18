#pragma once

// Data-driven achievement condition table. Additive to AchievementBus::emit()'s
// existing hand-coded switch (48 achievements, untouched) -- this table only
// covers achievements appended from AchievementId::IronStomach (48) onward.
// The condition-type enum below is the real ceiling on the eventual 200-300
// entry pool (see parent's amendment 1, msg 4090): every achievement idea has
// to be expressible as one row against one of these six types before it's
// authored, not after.

#include <cstdint>

#include "Achievements.h"
#include "GameTypes.h"

namespace game {

enum class ConditionType : uint8_t {
  CounterCompare,   // A live game-state counter vs a fixed value.
  EventFieldMatch,  // A single event whose payload field matches a value (fires once).
  EventCount,       // N occurrences of an event type within the run.
  NoEventInWindow,  // Absence of an event type across a window (run or current floor).
  ItemSpecific,     // An item-related event filtered by ItemType.
  Compound,         // AND of two other condition-table rows (by index).
};

// -- CounterCompare fields: live counters read straight off GAME_STATE / AchievementBus. --
enum class CounterField : uint8_t {
  Depth,
  Kills,
  Gold,
  TurnCount,
  Hunger,
  Hp,
  TilesWalked,       // AchievementBus::lifetimeTilesWalked
  FloorsFullyExplored,
};

enum class CompareOp : uint8_t {
  GreaterEqual,
  LessEqual,
  Equal,
};

// -- EventFieldMatch fields: which GameEvent payload field to compare. --
enum class EventField : uint8_t {
  Damage,
  HpAfter,
  MonsterMaxHp,
  MonsterMinDepth,
  NewLevel,
};

enum class NoEventWindow : uint8_t {
  Run,
  Floor,
};

struct AchievementCondition {
  AchievementId id;
  ConditionType type;

  // CounterCompare
  CounterField field = CounterField::Depth;
  CompareOp op = CompareOp::GreaterEqual;
  int32_t value = 0;

  // EventFieldMatch / EventCount / NoEventInWindow / ItemSpecific: which event
  // this row reacts to.
  GameEventType eventType = GameEventType::MonsterKilled;

  // EventFieldMatch: field to compare, reuses op/value above.
  EventField eventField = EventField::Damage;

  // ItemSpecific: item type filter, reuses value above as a count threshold
  // (a threshold of 1 == fires on first occurrence).
  ItemType itemType = ItemType::ItemTypeCount;

  // NoEventInWindow: which window to watch.
  NoEventWindow window = NoEventWindow::Run;

  // Compound: AND of two other rows, by index into CONDITIONS below. -1 == unused.
  int8_t compoundA = -1;
  int8_t compoundB = -1;
};

// Demo table: 2 rows per condition type (12 total), proving the enum covers
// a normal case and a "deliberately stupid" case for each type per amendment 1.
// Row order matches AchievementId declaration order (48-59); index into this
// array is also used as the per-row runtime-state slot for EventCount and
// NoEventInWindow (see AchievementBus::conditionEventCounts_/conditionWindowBroken_).
constexpr AchievementCondition CONDITIONS[] = {
    // -- CounterCompare --
    {AchievementId::IronStomach, ConditionType::CounterCompare, CounterField::Hunger,
     CompareOp::GreaterEqual, 9000},  // silly: hunger stat has no real cap this high, proves the type tolerates nonsense thresholds
    {AchievementId::StepCounter, ConditionType::CounterCompare, CounterField::TilesWalked,
     CompareOp::GreaterEqual, 500},

    // -- EventFieldMatch --
    {AchievementId::OneShot, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 9999, GameEventType::MonsterKilled, EventField::Damage},
    {AchievementId::NickOfTime, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::Equal, 1, GameEventType::PlayerDamaged, EventField::HpAfter},

    // -- EventCount --
    {AchievementId::SerialKiller, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 25, GameEventType::MonsterKilled},
    {AchievementId::Klepto, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 100, GameEventType::ItemPickedUp},  // silly: near-every-tile pickup spam

    // -- NoEventInWindow --
    {AchievementId::Ghost, ConditionType::NoEventInWindow, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::PlayerDamaged, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run},
    {AchievementId::Untroubled, ConditionType::NoEventInWindow, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::PlayerDamaged, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Floor},

    // -- ItemSpecific --
    {AchievementId::PotionChugger, ConditionType::ItemSpecific, CounterField::Depth,
     CompareOp::GreaterEqual, 10, GameEventType::ItemUsed, EventField::Damage, ItemType::Potion},
    {AchievementId::ScrollHoarder, ConditionType::ItemSpecific, CounterField::Depth,
     CompareOp::GreaterEqual, 5, GameEventType::ItemPickedUp, EventField::Damage, ItemType::Scroll},

    // -- Compound (AND of two prior rows by index) --
    {AchievementId::GreedyAndFast, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 1, 4},  // StepCounter(1) AND SerialKiller(4)
    {AchievementId::DeepAndDeadly, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 0, 5},  // IronStomach(0) AND Klepto(5)
};

constexpr uint8_t CONDITION_COUNT = sizeof(CONDITIONS) / sizeof(CONDITIONS[0]);

}  // namespace game
