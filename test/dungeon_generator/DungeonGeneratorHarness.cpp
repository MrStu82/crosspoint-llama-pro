// Standalone host harness (no gtest dependency -- compiled directly with g++)
// for Phase 7 reqs 1 and 4 (World Dungeon: Correctness), plus a cheap
// determinism regression guard for req 5's assumptions at the dungeon-gen
// layer. Mirrors DungeonGeneratorTest.cpp's gtest version 1:1 in intent;
// this file exists because neither the container nor Trantor currently has
// a working cmake+gtest+native-compiler combo (container: no cmake; Trantor:
// no native g++, only the ESP32 cross toolchain) -- see .planning/07-progress.md.
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra -I src -I test/dungeon_generator/stubs
//        test/dungeon_generator/DungeonGeneratorHarness.cpp
//        src/game/DungeonGenerator.cpp -o /tmp/dg_harness
// Run:   /tmp/dg_harness

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "game/DungeonGenerator.h"
#include "game/GameTypes.h"

namespace {

using game::Item;
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

  GeneratedLevel()
      : tiles(game::MAP_SIZE), monsters(MAX_MONSTERS), items(MAX_ITEMS_PER_LEVEL) {}
};

GeneratedLevel generateLevel(uint32_t seed, uint8_t depth) {
  GeneratedLevel level;
  level.result =
      DungeonGenerator::generate(seed, depth, level.tiles.data(), level.monsters.data(), level.items.data());
  return level;
}

void testNoRingOfPowerInRandomLoot() {
  constexpr int kFloors = 10000;
  int totalItems = 0;
  int ringHits = 0;

  for (int i = 0; i < kFloors; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + 1;
    uint8_t depth = static_cast<uint8_t>(1 + (i % MAX_DEPTH));

    GeneratedLevel level = generateLevel(seed, depth);
    totalItems += level.result.itemCount;

    for (int j = 0; j < level.result.itemCount; j++) {
      const Item& item = level.items[j];
      bool isRingOfPower =
          item.type == static_cast<uint8_t>(game::ItemType::Ring) && item.subtype == 0;
      if (isRingOfPower) {
        ringHits++;
        std::fprintf(stderr, "  ring of power at floor %d (seed=%u, depth=%d)\n", i, seed,
                     static_cast<int>(depth));
      }
    }
  }

  CHECK(ringHits == 0, "Ring of Power appeared %d time(s) in random loot across %d floors", ringHits,
        kFloors);
  CHECK(totalItems > kFloors, "Suspiciously few items generated across the sweep (%d total) -- "
                              "loot placement may be broken, not just untested.",
        totalItems);
  std::printf("[req1] 10000-floor loot sweep: %d items generated, %d Ring-of-Power hits\n", totalItems,
              ringHits);
}

void testMonsterSelectionIsDepthWeighted() {
  constexpr int kFloors = 1000;
  constexpr uint8_t kShallowDepth = 5;
  constexpr uint8_t kDeepDepth = 26;
  static_assert(kDeepDepth == MAX_DEPTH, "test assumes depth 26 is MAX_DEPTH per the frozen req");

  auto meanMonsterTier = [](uint8_t depth) {
    double sum = 0;
    int count = 0;
    for (int i = 0; i < kFloors; i++) {
      uint32_t seed = static_cast<uint32_t>(i) * 40503u + 7;
      GeneratedLevel level = generateLevel(seed, depth);
      for (int j = 0; j < level.result.monsterCount; j++) {
        sum += level.monsters[j].type;
        count++;
      }
    }
    return count > 0 ? sum / count : 0.0;
  };

  double shallowMean = meanMonsterTier(kShallowDepth);
  double deepMean = meanMonsterTier(kDeepDepth);

  CHECK(deepMean > shallowMean,
        "depth %d mean monster tier (%.3f) is not greater than depth %d mean tier (%.3f)",
        static_cast<int>(kDeepDepth), deepMean, static_cast<int>(kShallowDepth), shallowMean);
  std::printf("[req4] depth %d mean tier=%.3f, depth %d mean tier=%.3f (1000 floors each)\n",
              static_cast<int>(kShallowDepth), shallowMean, static_cast<int>(kDeepDepth), deepMean);
}

void testSameSeedAndDepthProduceIdenticalLevel() {
  GeneratedLevel a = generateLevel(12345, 10);
  GeneratedLevel b = generateLevel(12345, 10);

  CHECK(a.result.monsterCount == b.result.monsterCount, "monsterCount mismatch: %d vs %d",
        a.result.monsterCount, b.result.monsterCount);
  CHECK(a.result.itemCount == b.result.itemCount, "itemCount mismatch: %d vs %d", a.result.itemCount,
        b.result.itemCount);
  CHECK(a.result.stairsDownX == b.result.stairsDownX && a.result.stairsDownY == b.result.stairsDownY,
        "stairsDown mismatch: (%d,%d) vs (%d,%d)", a.result.stairsDownX, a.result.stairsDownY,
        b.result.stairsDownX, b.result.stairsDownY);
  CHECK(a.tiles == b.tiles, "tile grids differ for identical (seed, depth)");
  std::printf("[req5-adjacent] same seed+depth determinism check done\n");
}

}  // namespace

int main() {
  testNoRingOfPowerInRandomLoot();
  testMonsterSelectionIsDepthWeighted();
  testSameSeedAndDepthProduceIdenticalLevel();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
