// Standalone host harness (no gtest dependency -- compiled directly with g++)
// for Phase 11 work item 5 follow-up: proves every generated floor contains
// at least one Food item, across a large seed sweep and all 26 depths --
// parent's explicit ruling ("guarantee food... prove it in the harness
// against a seed sweep, not one happy seed").
//
// Unlike HungerEscapabilityHarness.cpp (which re-implements the hunger
// tick/regen formulas by hand), this harness links against the REAL
// DungeonGenerator.cpp -- same pattern as DungeonGeneratorHarness.cpp -- so
// this is a proof about the actual shipped placeItems() logic, not a
// reimplementation of it.
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra -I src -I test/dungeon_generator/stubs
//        test/dungeon_generator/GuaranteedFoodHarness.cpp
//        src/game/DungeonGenerator.cpp -o /tmp/food_harness
// Run:   /tmp/food_harness

#include <cstdio>
#include <vector>

#include "game/DungeonGenerator.h"
#include "game/GameTypes.h"

namespace {

using game::Item;
using game::ItemType;
using game::MAX_DEPTH;
using game::MAX_ITEMS_PER_LEVEL;
using game::MAX_MONSTERS;
using game::Monster;
using game::Tile;

int failures = 0;

#define CHECK(cond, ...)                          \
  do {                                             \
    if (!(cond)) {                                 \
      std::fprintf(stderr, "FAIL: " __VA_ARGS__);  \
      std::fprintf(stderr, "\n");                  \
      failures++;                                  \
    }                                              \
  } while (0)

struct GeneratedLevel {
  std::vector<Tile> tiles;
  std::vector<Monster> monsters;
  std::vector<Item> items;
  DungeonGenerator::Result result;

  GeneratedLevel() : tiles(game::MAP_SIZE), monsters(MAX_MONSTERS), items(MAX_ITEMS_PER_LEVEL) {}
};

GeneratedLevel generateLevel(uint32_t seed, uint8_t depth) {
  GeneratedLevel level;
  level.result =
      DungeonGenerator::generate(seed, depth, level.tiles.data(), level.monsters.data(), level.items.data());
  return level;
}

int countFoodItems(const GeneratedLevel& level) {
  int count = 0;
  for (int j = 0; j < level.result.itemCount; j++) {
    if (level.items[j].type == static_cast<uint8_t>(ItemType::Food)) count++;
  }
  return count;
}

// Core requirement: every floor at every depth 1..26, across a wide seed
// sweep, contains at least one Food item -- not just "usually", "on average",
// or "for a typical seed". A single miss anywhere in the sweep is a real bug,
// since the hunger clock's escapability guarantee depends on this holding
// unconditionally.
void testEveryFloorHasFoodAcrossSeedSweep() {
  constexpr int kSeedsPerDepth = 500;
  int totalFloors = 0;
  int floorsWithNoFood = 0;
  int totalFoodItems = 0;

  for (uint8_t depth = 1; depth <= MAX_DEPTH; depth++) {
    for (int i = 0; i < kSeedsPerDepth; i++) {
      // Deliberately not a trivially-sequential seed -- spread across the
      // full uint32_t range so this isn't accidentally testing one narrow
      // band of the RNG's behavior.
      uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + static_cast<uint32_t>(depth) * 40503u + 1;

      GeneratedLevel level = generateLevel(seed, depth);
      totalFloors++;

      int foodCount = countFoodItems(level);
      totalFoodItems += foodCount;
      if (foodCount == 0) {
        floorsWithNoFood++;
        std::fprintf(stderr, "  NO FOOD: depth=%d seed=%u (itemCount=%d)\n", static_cast<int>(depth), seed,
                     level.result.itemCount);
      }
    }
  }

  CHECK(floorsWithNoFood == 0,
        "%d of %d generated floors had zero Food items -- hunger clock is not provably escapable",
        floorsWithNoFood, totalFloors);
  std::printf(
      "[req5-food] %d floors across depths 1-%d (%d seeds/depth): %d had zero food, %d total food items "
      "placed\n",
      totalFloors, static_cast<int>(MAX_DEPTH), kSeedsPerDepth, floorsWithNoFood, totalFoodItems);
}

// Confirms the guarantee holds even under item-count pressure: at MAX_DEPTH
// the random-roll count is at its highest (2 + depth, clamped to
// MAX_ITEMS_PER_LEVEL), so the guaranteed food slot is the one most likely to
// get crowded out by the cap if the placement order were ever wrong (i.e. if
// food were appended AFTER the random roll instead of forced in first).
void testFoodSurvivesAtMaxItemDensity() {
  constexpr int kSeeds = 500;
  int floorsWithNoFood = 0;

  for (int i = 0; i < kSeeds; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 1000003u + 99;
    GeneratedLevel level = generateLevel(seed, MAX_DEPTH);
    CHECK(level.result.itemCount <= MAX_ITEMS_PER_LEVEL, "itemCount %d exceeds MAX_ITEMS_PER_LEVEL %d",
          level.result.itemCount, MAX_ITEMS_PER_LEVEL);
    if (countFoodItems(level) == 0) floorsWithNoFood++;
  }

  CHECK(floorsWithNoFood == 0, "%d of %d MAX_DEPTH floors (highest item-count pressure) had zero food",
        floorsWithNoFood, kSeeds);
  std::printf("[req5-food] MAX_DEPTH (%d) item-density check: %d/%d floors missing food\n",
              static_cast<int>(MAX_DEPTH), floorsWithNoFood, kSeeds);
}

// Sanity check that this isn't a vacuous test: confirm Ring of Power / Master
// Key still never appear in random loot (regression guard shared with
// DungeonGeneratorHarness.cpp's existing req1 check) -- proves the guaranteed
// food slot didn't accidentally break the existing quest-item exclusion.
void testQuestItemsStillExcludedFromRandomLoot() {
  constexpr int kFloors = 2000;
  int ringHits = 0;
  int keyHits = 0;

  for (int i = 0; i < kFloors; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + 1;
    uint8_t depth = static_cast<uint8_t>(1 + (i % MAX_DEPTH));
    GeneratedLevel level = generateLevel(seed, depth);

    for (int j = 0; j < level.result.itemCount; j++) {
      const Item& item = level.items[j];
      if (item.type == static_cast<uint8_t>(ItemType::Ring) && item.subtype == 0) ringHits++;
      if (item.type == static_cast<uint8_t>(ItemType::Amulet) && item.subtype == 0) keyHits++;
    }
  }

  CHECK(ringHits == 0 && keyHits == 0,
        "quest items leaked into random loot after the food-guarantee change: %d ring, %d key hits",
        ringHits, keyHits);
  std::printf("[req5-food] %d-floor quest-item exclusion regression check: %d ring hits, %d key hits\n",
              kFloors, ringHits, keyHits);
}

}  // namespace

int main() {
  testEveryFloorHasFoodAcrossSeedSweep();
  testFoodSurvivesAtMaxItemDensity();
  testQuestItemsStillExcludedFromRandomLoot();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
