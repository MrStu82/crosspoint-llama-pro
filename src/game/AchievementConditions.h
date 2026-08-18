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

    // -- Combat bucket (Milestone 2, ids 90-124). Row index below == position
    // in this array (29 onward), which is also what Group E's Compound rows
    // reference (compoundA reuses the existing Depth rows above; compoundB
    // references Bloodless at index 59). --

    // Group A: kill-count tiers (EventCount / MonsterKilled).
    {AchievementId::Bloodletter, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 5, GameEventType::MonsterKilled},    // index 29
    {AchievementId::Cutthroat, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 15, GameEventType::MonsterKilled},   // index 30
    {AchievementId::HuntersTally, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 35, GameEventType::MonsterKilled},   // index 31
    {AchievementId::BodyCount, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 60, GameEventType::MonsterKilled},   // index 32
    {AchievementId::Warpath, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 90, GameEventType::MonsterKilled},   // index 33
    {AchievementId::Slaughterhouse, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 130, GameEventType::MonsterKilled},  // index 34
    {AchievementId::KillStreak, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 180, GameEventType::MonsterKilled},  // index 35
    {AchievementId::Merciless, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 250, GameEventType::MonsterKilled},  // index 36
    {AchievementId::GenocideRun, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 350, GameEventType::MonsterKilled},  // index 37
    {AchievementId::ApexPredator, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 500, GameEventType::MonsterKilled},  // index 38

    // Group B: single-hit damage tiers (EventFieldMatch / MonsterKilled / Damage).
    {AchievementId::FirstCut, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 12, GameEventType::MonsterKilled, EventField::Damage},   // index 39
    {AchievementId::HeavyHand, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 20, GameEventType::MonsterKilled, EventField::Damage},   // index 40
    {AchievementId::BoneBreaker, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 30, GameEventType::MonsterKilled, EventField::Damage},   // index 41
    {AchievementId::CrushingBlow, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 45, GameEventType::MonsterKilled, EventField::Damage},   // index 42
    {AchievementId::Devastator, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 65, GameEventType::MonsterKilled, EventField::Damage},   // index 43
    {AchievementId::AnnihilatingStrike, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 90, GameEventType::MonsterKilled, EventField::Damage},   // index 44
    {AchievementId::WorldEnder, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 120, GameEventType::MonsterKilled, EventField::Damage},  // index 45
    {AchievementId::Cataclysm, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 160, GameEventType::MonsterKilled, EventField::Damage},  // index 46

    // Group C: hits-survived tiers (EventCount / PlayerDamaged).
    {AchievementId::BruisedNotBroken, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 10, GameEventType::PlayerDamaged},  // index 47
    {AchievementId::PunchingBag, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 20, GameEventType::PlayerDamaged},  // index 48
    {AchievementId::GluttonForPunishment, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 35, GameEventType::PlayerDamaged},  // index 49
    {AchievementId::Besieged, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 50, GameEventType::PlayerDamaged},  // index 50
    {AchievementId::LastOneStanding, ConditionType::EventCount, CounterField::Depth,
     CompareOp::GreaterEqual, 75, GameEventType::PlayerDamaged},  // index 51

    // Group D: out-levelled-kill tiers (EventFieldMatch / MonsterKilled / MonsterMinDepth).
    {AchievementId::Overmatched, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 10, GameEventType::MonsterKilled, EventField::MonsterMinDepth},  // index 52
    {AchievementId::Outgunned, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 14, GameEventType::MonsterKilled, EventField::MonsterMinDepth},  // index 53
    {AchievementId::InOverYourHead, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 18, GameEventType::MonsterKilled, EventField::MonsterMinDepth},  // index 54
    {AchievementId::DavidAndGoliath, ConditionType::EventFieldMatch, CounterField::Depth,
     CompareOp::GreaterEqual, 22, GameEventType::MonsterKilled, EventField::MonsterMinDepth},  // index 55

    // Group F: thrown-weapon tiers (ItemSpecific / ItemThrown / Weapon).
    {AchievementId::QuickDraw, ConditionType::ItemSpecific, CounterField::Depth,
     CompareOp::GreaterEqual, 5, GameEventType::ItemThrown, EventField::Damage, ItemType::Weapon},   // index 56
    {AchievementId::RangedSpecialist, ConditionType::ItemSpecific, CounterField::Depth,
     CompareOp::GreaterEqual, 15, GameEventType::ItemThrown, EventField::Damage, ItemType::Weapon},  // index 57
    {AchievementId::MasterMarksman, ConditionType::ItemSpecific, CounterField::Depth,
     CompareOp::GreaterEqual, 30, GameEventType::ItemThrown, EventField::Damage, ItemType::Weapon},  // index 58

    // Group E: pacifist descent. Bloodless is the standalone NoEventInWindow
    // row; the remaining four are Compound rows AND-ing an existing Depth row
    // (from the Depth bucket above) with Bloodless (index 59).
    {AchievementId::Bloodless, ConditionType::NoEventInWindow, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run},  // index 59

    {AchievementId::PacifistToSix, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 14, 59},   // Depth6(14) AND Bloodless(59), index 60
    {AchievementId::PacifistToEleven, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 18, 59},   // Depth11(18) AND Bloodless(59), index 61
    {AchievementId::PacifistToSixteen, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 22, 59},   // Depth16(22) AND Bloodless(59), index 62
    {AchievementId::PeacefulSovereign, ConditionType::Compound, CounterField::Depth,
     CompareOp::GreaterEqual, 0, GameEventType::MonsterKilled, EventField::Damage,
     ItemType::ItemTypeCount, NoEventWindow::Run, 26, 59},   // Depth21(26) AND Bloodless(59), index 63
};

constexpr uint8_t CONDITION_COUNT = sizeof(CONDITIONS) / sizeof(CONDITIONS[0]);

}  // namespace game
