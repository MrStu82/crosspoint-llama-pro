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

    // -- Depth bucket (Milestone 1, ids 60-89): CounterCompare singles for
    // every remaining depth threshold (1-26 minus the 9 legacy hand-coded
    // depths), then Compound rows pairing a depth single with an existing
    // "normal" demo-pool condition row (StepCounter/SerialKiller/Ghost/
    // Untroubled/PotionChugger/ScrollHoarder/Klepto). Deliberately excludes
    // the "silly" demo rows (IronStomach, OneShot) as compound partners --
    // this bucket is meant to read as real achievements, not proof-of-type
    // gags. Row index below == position in this array (12 onward), which is
    // also what each Compound row's compoundA/compoundB reference. --
    {AchievementId::Depth3, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 3},   // index 12
    {AchievementId::Depth4, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 4},   // index 13
    {AchievementId::Depth6, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 6},   // index 14
    {AchievementId::Depth7, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 7},   // index 15
    {AchievementId::Depth8, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 8},   // index 16
    {AchievementId::Depth9, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 9},   // index 17
    {AchievementId::Depth11, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 11},  // index 18
    {AchievementId::Depth12, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 12},  // index 19
    {AchievementId::Depth13, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 13},  // index 20
    {AchievementId::Depth14, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 14},  // index 21
    {AchievementId::Depth16, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 16},  // index 22
    {AchievementId::Depth17, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 17},  // index 23
    {AchievementId::Depth18, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 18},  // index 24
    {AchievementId::Depth19, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 19},  // index 25
    {AchievementId::Depth21, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 21},  // index 26
    {AchievementId::Depth23, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 23},  // index 27
    {AchievementId::Depth24, ConditionType::CounterCompare, CounterField::Depth,
     CompareOp::GreaterEqual, 24},  // index 28

    {AchievementId::TreaderOfShallows, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 12, 1},   // Depth3(12) AND StepCounter(1)
    {AchievementId::ButcherOfSix, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 14, 4},   // Depth6(14) AND SerialKiller(4)
    {AchievementId::UnscathedEighth, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 16, 6},   // Depth8(16) AND Ghost(6)
    {AchievementId::SereneNinth, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 17, 7},   // Depth9(17) AND Untroubled(7)
    {AchievementId::AlchemistOfEleven, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 18, 8},   // Depth11(18) AND PotionChugger(8)
    {AchievementId::ScribeOfTwelve, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 19, 9},   // Depth12(19) AND ScrollHoarder(9)
    {AchievementId::HoarderOfThirteen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 20, 5},   // Depth13(20) AND Klepto(5)
    {AchievementId::WandererOfFourteen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 21, 1},   // Depth14(21) AND StepCounter(1)
    {AchievementId::ReaperOfSixteen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 22, 4},   // Depth16(22) AND SerialKiller(4)
    {AchievementId::PhantomOfSeventeen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 23, 6},   // Depth17(23) AND Ghost(6)
    {AchievementId::UnbrokenNineteen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 25, 7},   // Depth19(25) AND Untroubled(7)
    {AchievementId::BrewmasterOfTwentyOne, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 26, 8},   // Depth21(26) AND PotionChugger(8)
    {AchievementId::LoremasterOfTwentyThree, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 27, 9},   // Depth23(27) AND ScrollHoarder(9)
};

constexpr uint8_t CONDITION_COUNT = sizeof(CONDITIONS) / sizeof(CONDITIONS[0]);

}  // namespace game
