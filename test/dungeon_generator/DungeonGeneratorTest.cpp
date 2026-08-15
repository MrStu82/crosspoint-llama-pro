// Host harness for Phase 7 reqs 1 and 4 (World Dungeon: Correctness).
//
// Req 1: across 10,000 generated floors, the random loot roll never produces
// a Ring of Power (the quest item -- it must only ever come from the boss's
// death drop, never DungeonGenerator::placeItems()'s uniform roll).
//
// Req 4: monster selection is depth-weighted -- deeper floors should skew
// toward tougher (higher MONSTER_DEFS-index) monsters than shallow floors.
// Verified as: mean monster-def-index at depth 26 > mean monster-def-index
// at depth 5, over 1,000 floors each.

#include <gtest/gtest.h>

#include <vector>

#include "game/DungeonGenerator.h"
#include "game/GameTypes.h"

namespace {

using game::Item;
using game::ITEM_DEF_COUNT;
using game::MAX_DEPTH;
using game::MAX_ITEMS_PER_LEVEL;
using game::MAX_MONSTERS;
using game::Monster;
using game::RING_OF_POWER_DEF;
using game::Tile;

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

}  // namespace

TEST(DungeonGenerator, NoRingOfPowerInRandomLootAcross10000Floors) {
  constexpr int kFloors = 10000;
  int totalItems = 0;

  for (int i = 0; i < kFloors; i++) {
    // Vary seed and depth so this isn't just the same floor generated 10,000 times.
    uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + 1;
    uint8_t depth = static_cast<uint8_t>(1 + (i % MAX_DEPTH));

    GeneratedLevel level = generateLevel(seed, depth);
    totalItems += level.result.itemCount;

    for (int j = 0; j < level.result.itemCount; j++) {
      const Item& item = level.items[j];
      // RING_OF_POWER_DEF's defining fields (see GameTypes.h ITEM_DEFS table):
      // type == Ring, subtype == 0. placeItems() should never produce this
      // combination via its random roll.
      bool isRingOfPower =
          item.type == static_cast<uint8_t>(game::ItemType::Ring) && item.subtype == 0;
      ASSERT_FALSE(isRingOfPower) << "Ring of Power found in random loot at floor " << i
                                   << " (seed=" << seed << ", depth=" << static_cast<int>(depth) << ")";
    }
  }

  // Sanity check the sweep actually generated a meaningful sample, not silently zero items.
  ASSERT_GT(totalItems, kFloors) << "Suspiciously few items generated across the sweep -- "
                                     "loot placement may be broken, not just untested.";
}

TEST(DungeonGenerator, MonsterSelectionIsDepthWeightedOver1000Floors) {
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
        // MONSTER_DEFS is authored in roughly tier-ascending order (see
        // DungeonGenerator.cpp's placeMonsters() comment) -- the def index
        // itself is the tier proxy this req is checking.
        sum += level.monsters[j].type;
        count++;
      }
    }
    return count > 0 ? sum / count : 0.0;
  };

  double shallowMean = meanMonsterTier(kShallowDepth);
  double deepMean = meanMonsterTier(kDeepDepth);

  ASSERT_GT(deepMean, shallowMean) << "depth " << static_cast<int>(kDeepDepth)
                                    << " mean monster tier (" << deepMean << ") is not greater than depth "
                                    << static_cast<int>(kShallowDepth) << " mean tier (" << shallowMean << ")";
}

TEST(DungeonGenerator, SameSeedAndDepthProduceIdenticalLevel) {
  // Not one of the 7 numbered reqs directly, but a cheap regression guard for
  // req 5 (deterministic RNG) at the dungeon-generation layer: same seed +
  // depth must always produce the same layout, since GameState relies on
  // DungeonGenerator::generate() being a pure function of (gameSeed, depth).
  GeneratedLevel a = generateLevel(12345, 10);
  GeneratedLevel b = generateLevel(12345, 10);

  EXPECT_EQ(a.result.monsterCount, b.result.monsterCount);
  EXPECT_EQ(a.result.itemCount, b.result.itemCount);
  EXPECT_EQ(a.result.stairsDownX, b.result.stairsDownX);
  EXPECT_EQ(a.result.stairsDownY, b.result.stairsDownY);
  EXPECT_EQ(a.tiles, b.tiles);
}
