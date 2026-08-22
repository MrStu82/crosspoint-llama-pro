#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "game/FrameDirtyPlanner.h"

int main() {
  using namespace game;
  constexpr int cols = 34;
  constexpr int rows = 11;
  constexpr int iterations = 20000;

  Tile tiles[MAP_SIZE];
  uint8_t fog[FOG_SIZE];
  bool visible[MAP_SIZE];
  Monster monsters[MAX_MONSTERS]{};
  Item items[MAX_ITEMS_PER_LEVEL]{};
  std::fill_n(tiles, MAP_SIZE, Tile::Floor);
  std::memset(fog, 0xFF, sizeof(fog));
  std::fill_n(visible, MAP_SIZE, true);
  for (int i = 0; i < MAX_MONSTERS; i++) {
    monsters[i].x = static_cast<int16_t>((i * 7) % MAP_WIDTH);
    monsters[i].y = static_cast<int16_t>((i * 11) % MAP_HEIGHT);
    monsters[i].type = static_cast<uint8_t>(i % MONSTER_DEF_COUNT);
    monsters[i].hp = 1;
  }
  for (int i = 0; i < MAX_ITEMS_PER_LEVEL; i++) {
    items[i].x = static_cast<int16_t>((i * 13) % MAP_WIDTH);
    items[i].y = static_cast<int16_t>((i * 17) % MAP_HEIGHT);
    items[i].type = static_cast<uint8_t>(i % static_cast<int>(ItemType::ItemTypeCount));
  }

  FrameDirtyPlanner planner;
  volatile uint64_t checksum = 0;
  auto started = std::chrono::steady_clock::now();
  for (int n = 0; n < iterations; n++) {
    for (int row = 0; row < rows; row++) {
      for (int col = 0; col < cols; col++) {
        const CellVisual cv = planner.computeCellVisual(col, row, tiles, fog, monsters, MAX_MONSTERS,
                                                        items, MAX_ITEMS_PER_LEVEL, visible, 17, 5);
        checksum = checksum + cv.glyph + cv.visState;
      }
    }
  }
  const auto legacyUs = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count();

  PlannerLayout layout{cols, rows, 2, 28, 14, 20, 480, 26, 416, 194};
  started = std::chrono::steady_clock::now();
  for (int n = 0; n < iterations; n++) {
    const FramePlan plan = planner.planFrame(layout, 17, 5, "HP", "MP", "D", "L", "Fed", "a", "b",
                                              tiles, fog, monsters, MAX_MONSTERS, items,
                                              MAX_ITEMS_PER_LEVEL, visible, &kThemeDefault);
    checksum = checksum + plan.windowCount;
  }
  const auto indexedUs = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count();

  std::printf("Planner hot path: legacy=%lld us indexed=%lld us delta=%.1f%% checksum=%llu\n",
              static_cast<long long>(legacyUs), static_cast<long long>(indexedUs),
              legacyUs == 0 ? 0.0 : 100.0 * (legacyUs - indexedUs) / legacyUs,
              static_cast<unsigned long long>(checksum));
  return indexedUs < legacyUs ? 0 : 1;
}
