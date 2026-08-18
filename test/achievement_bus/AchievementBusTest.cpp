// Proof, per parent's explicit ask (2026-08-18): force an ItemUsed and an
// ItemPickedUp through the real AchievementBus::emit()/evaluateConditions()
// path and show a previously-dead ItemSpecific entry (PotionChugger) actually
// unlocking, driven by the itemType fix in GameActivity.cpp/
// GameMenuActivity.cpp. This compiles and links the REAL AchievementBus.cpp,
// AchievementConditions.h and Achievements.h from src/game/ -- only
// HalStorage/Serialization/Logging (Arduino-dependent) and GameState's .cpp
// (also Arduino-dependent) are swapped for host stubs; the condition table,
// the draw guard, and the ItemSpecific matching logic under test are all
// untouched production code.

#include <gtest/gtest.h>

#include "AchievementBus.h"
#include "Achievements.h"
#include "GameState.h"
#include "GameTypes.h"

namespace {

game::GameEvent makeItemUsedEvent(game::ItemType type) {
  game::GameEvent event{};
  event.type = game::GameEventType::ItemUsed;
  event.itemType = type;
  return event;
}

game::GameEvent makeItemPickedUpEvent(game::ItemType type) {
  game::GameEvent event{};
  event.type = game::GameEventType::ItemPickedUp;
  event.itemType = type;
  return event;
}

}  // namespace

// PotionChugger (AchievementConditions.h): ItemSpecific, ItemUsed,
// itemType==Potion, threshold 10. Before the itemType fix, every ItemUsed
// event arrived with itemType defaulted to ItemTypeCount, so
// `event.itemType != c.itemType` was always true and this achievement could
// never fire for any player, ever, no matter how many potions they drank.
TEST(AchievementBusItemTypeFix, PotionChuggerUnlocksOnTenPotionUses) {
  AchievementBus& bus = ACHIEVEMENTS;
  bus.load();  // clean slate: stub storage has no file, so this just zeroes state.

  ASSERT_FALSE(bus.isUnlocked(game::AchievementId::PotionChugger))
      << "precondition: must start locked for this test to prove anything";

  // PotionChugger is subject to AchievementBus's per-run draw guard (a random
  // 50-of-N subset of the whole id space, redrawn every resetRun()) -- so
  // this loops across simulated runs, exactly like a player who keeps
  // starting new games, until a run draws it. 50/~250 is roughly a 1-in-5
  // chance per run; 500 attempts is comfortably beyond any plausible bad-luck
  // streak while staying well under a second of wall clock.
  bool unlocked = false;
  for (int attempt = 0; attempt < 500 && !unlocked; attempt++) {
    bus.resetRun();
    for (int use = 0; use < 10; use++) {
      bus.emit(makeItemUsedEvent(game::ItemType::Potion));
    }
    unlocked = bus.isUnlocked(game::AchievementId::PotionChugger);
  }

  EXPECT_TRUE(unlocked)
      << "PotionChugger never unlocked across 500 simulated runs of 10 "
         "Potion ItemUsed events each -- the itemType fix is not firing";
}

// Companion check for the other repaired event type, per parent's explicit
// ask to prove both ItemUsed and ItemPickedUp. ScrollHoarder (also
// ItemSpecific, ItemPickedUp, itemType==Scroll, threshold 5) was dead for
// exactly the same reason as PotionChugger.
TEST(AchievementBusItemTypeFix, ScrollHoarderUnlocksOnFiveScrollPickups) {
  AchievementBus& bus = ACHIEVEMENTS;
  bus.load();

  ASSERT_FALSE(bus.isUnlocked(game::AchievementId::ScrollHoarder));

  bool unlocked = false;
  for (int attempt = 0; attempt < 500 && !unlocked; attempt++) {
    bus.resetRun();
    for (int pickup = 0; pickup < 5; pickup++) {
      bus.emit(makeItemPickedUpEvent(game::ItemType::Scroll));
    }
    unlocked = bus.isUnlocked(game::AchievementId::ScrollHoarder);
  }

  EXPECT_TRUE(unlocked)
      << "ScrollHoarder never unlocked across 500 simulated runs of 5 "
         "Scroll ItemPickedUp events each -- the itemType fix is not firing";
}

// Negative control: a mismatched itemType must never satisfy an ItemSpecific
// condition. This is what guarantees the two tests above are actually
// exercising the itemType comparison added by the fix, not some other path
// (e.g. a threshold of 0, or the draw guard being bypassed).
TEST(AchievementBusItemTypeFix, WrongItemTypeNeverUnlocksPotionChugger) {
  AchievementBus& bus = ACHIEVEMENTS;
  bus.load();

  for (int attempt = 0; attempt < 500; attempt++) {
    bus.resetRun();
    for (int use = 0; use < 10; use++) {
      bus.emit(makeItemUsedEvent(game::ItemType::Scroll));
    }
  }

  EXPECT_FALSE(bus.isUnlocked(game::AchievementId::PotionChugger))
      << "PotionChugger unlocked from Scroll ItemUsed events -- the itemType "
         "filter isn't discriminating types, it's just not blocking anymore";
}
