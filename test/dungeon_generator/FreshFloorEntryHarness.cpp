// Standalone host harness (no gtest dependency -- compiled directly with g++), reproducing
// GameActivity::onEnter()'s exact real-device sequence for a level with no existing save file:
//   loadOrGenerateLevel() [no-file branch: p.x = result.stairsUpX; p.y = result.stairsUpY;]
//   -> computeVisibility() [walks hasLineOfSight() over every tile within FOV_RADIUS of p.x/p.y]
// This is the "step after generation" boundary Phase 11's DungeonGeneratorHarness.cpp never
// drove (it calls DungeonGenerator::generate() only, never anything downstream of it) -- see
// the 2026-08-17 fresh clean-device panic report: generation logs successfully, then nothing,
// not even a frame. computeVisibility()/hasLineOfSight() are copied verbatim from
// GameActivity.cpp (private, not exposed via a header) rather than pulling in the rest of
// GameActivity's dependencies (input, rendering, save/load, combat) -- keeping this a
// two-stub harness (Logging.h from the existing dungeon_generator stub set, nothing new),
// per the explicit scope guardrail: no third mirror header.
//
// Build: g++ -std=c++20 -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
//        -Wall -Wextra -I src -I test/dungeon_generator/stubs \
//        test/dungeon_generator/FreshFloorEntryHarness.cpp \
//        src/game/DungeonGenerator.cpp -o /tmp/fresh_floor_harness
// Run:   /tmp/fresh_floor_harness

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "game/DungeonGenerator.h"
#include "game/FrameDirtyPlanner.h"
#include "game/GameTypes.h"

namespace {

using game::Item;
using game::MAX_DEPTH;
using game::MAX_ITEMS_PER_LEVEL;
using game::MAX_MONSTERS;
using game::Monster;
using game::Tile;

// --- Verbatim copy of GameActivity.cpp's private FOV logic (lines ~19-49, ~1116-1136) ---
// so the exact indexing this real device hit is what runs under ASan/UBSan here.

constexpr int FOV_RADIUS = 8;

bool hasLineOfSight(const game::Tile* tiles, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  int cx = x0, cy = y0;
  while (true) {
    if ((cx != x0 || cy != y0) && (cx != x1 || cy != y1)) {
      if (tiles[cy * game::MAP_WIDTH + cx] == game::Tile::Wall) {
        return false;
      }
    }
    if (cx == x1 && cy == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      cx += sx;
    }
    if (e2 <= dx) {
      err += dx;
      cy += sy;
    }
  }
  return true;
}

void computeVisibility(const game::Tile* tiles, int16_t px, int16_t py, bool* visible) {
  std::fill(visible, visible + game::MAP_SIZE, false);
  int startX = std::max(0, static_cast<int>(px) - FOV_RADIUS);
  int endX = std::min(game::MAP_WIDTH - 1, static_cast<int>(px) + FOV_RADIUS);
  int startY = std::max(0, static_cast<int>(py) - FOV_RADIUS);
  int endY = std::min(game::MAP_HEIGHT - 1, static_cast<int>(py) + FOV_RADIUS);
  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      int dx = x - px;
      int dy = y - py;
      if (dx * dx + dy * dy > FOV_RADIUS * FOV_RADIUS) continue;
      if (hasLineOfSight(tiles, px, py, x, y)) {
        visible[y * game::MAP_WIDTH + x] = true;
      }
    }
  }
}

int failures = 0;

#define CHECK(cond, ...)                          \
  do {                                             \
    if (!(cond)) {                                 \
      std::fprintf(stderr, "FAIL: " __VA_ARGS__);  \
      std::fprintf(stderr, "\n");                  \
      failures++;                                  \
    }                                              \
  } while (0)

// Layout constants copied from GameRenderer.h (CELL_W=14, CELL_H=20, STATUS_H=26,
// VIEWPORT_Y=STATUS_H+2, BANNER_H=56 reserved band, MESSAGE_H=MESSAGE_LINE_COUNT(8)*
// messageLineHeight(24, real UI_10_FONT_ID advanceY)+MESSAGE_PADDING_V(136)=328 as of
// the 2026-08-17 re-derived layout, CONTROLS_H=CONTROL_ROW_H*3) computed for the real
// x4pro screen (480x800, confirmed via test/corrupt_notice_hittest's existing
// initForTest(480, 800) usage) -- reproducing GameRenderer::computeLayout()'s numbers
// without needing GfxRenderer at all, since PlannerLayout is plain data.
game::PlannerLayout makeX4ProLayout() {
  constexpr int kScreenW = 480;
  constexpr int kScreenH = 800;
  constexpr int kCellW = 14;
  constexpr int kCellH = 20;
  constexpr int kStatusH = 26;
  constexpr int kViewportY = kStatusH + 2;
  constexpr int kBannerH = 56;
  constexpr int kMessageH = 8 * 24 + 136;
  constexpr int kControlsH = 56 * 3;  // CONTROL_ROW_H not exposed publicly; matches GameRenderer.h intent closely
                                       // enough for viewport sizing -- exact control row height doesn't affect
                                       // whether planFrame() indexes out of bounds, only how much viewport it grants.

  game::PlannerLayout layout;
  int viewportEndY = kScreenH - kBannerH - kMessageH - kControlsH;
  int viewportH = viewportEndY - kViewportY;
  int viewportW = kScreenW;
  layout.viewCols = viewportW / kCellW;
  layout.viewRows = viewportH / kCellH;
  layout.gridOffsetX = (viewportW - layout.viewCols * kCellW) / 2;
  layout.viewportY = kViewportY;
  layout.cellW = kCellW;
  layout.cellH = kCellH;
  layout.screenW = kScreenW;
  layout.statusH = kStatusH;
  layout.messageY = viewportEndY + kBannerH;
  layout.messageH = kMessageH;
  return layout;
}

void testFreshFloorEntryAcrossSeedsAndDepths() {
  constexpr int kFloors = 20000;
  std::vector<Tile> tiles(game::MAP_SIZE);
  std::vector<Monster> monsters(MAX_MONSTERS);
  std::vector<Item> items(MAX_ITEMS_PER_LEVEL);
  auto visibleStorage = std::make_unique<bool[]>(game::MAP_SIZE);  // vector<bool> has no .data()
  bool* visible = visibleStorage.get();
  std::vector<uint8_t> fogOfWar(game::FOG_SIZE, 0);

  game::PlannerLayout layout = makeX4ProLayout();
  game::FrameDirtyPlanner planner;

  for (int i = 0; i < kFloors; i++) {
    uint32_t seed = static_cast<uint32_t>(i) * 2654435761u + 1;
    uint8_t depth = static_cast<uint8_t>(1 + (i % MAX_DEPTH));

    DungeonGenerator::Result result =
        DungeonGenerator::generate(seed, depth, tiles.data(), monsters.data(), items.data());

    // Mirror GameActivity::loadOrGenerateLevel()'s no-existing-file branch exactly:
    // p.x = result.stairsUpX; p.y = result.stairsUpY; -- no bounds check on the assignment,
    // and none exists in the real code either.
    int16_t px = result.stairsUpX;
    int16_t py = result.stairsUpY;

    CHECK(px >= 0 && px < game::MAP_WIDTH, "seed=%u depth=%d: stairsUpX=%d out of [0,%d)", seed,
          static_cast<int>(depth), static_cast<int>(px), game::MAP_WIDTH);
    CHECK(py >= 0 && py < game::MAP_HEIGHT, "seed=%u depth=%d: stairsUpY=%d out of [0,%d)", seed,
          static_cast<int>(depth), static_cast<int>(py), game::MAP_HEIGHT);

    // The exact call GameActivity::onEnter() makes immediately after loadOrGenerateLevel(),
    // before any render -- this is where the real device panics with zero further log output.
    computeVisibility(tiles.data(), px, py, visible);

    // GameActivity::onEnter() then calls requestUpdate(), which drives render() ->
    // gameRenderer.draw() -> planFrame() as its very first statement (before clearScreen()
    // or any GfxRenderer call) -- FrameDirtyPlanner.h is explicitly documented as
    // zero-GfxRenderer-dependency, host-harness-drivable pure logic (candidate #3 from
    // parent's msg 3972 priority list). Reset planner state each floor to mirror a fresh
    // GameActivity::onEnter() (invalidate() forces fullClear on floor entry in the real code,
    // via loadOrGenerateLevel() implicitly changing tiles/entities under an unchanged
    // FrameDirtyPlanner instance -- exercised here explicitly via invalidate()).
    planner.invalidate();
    char hpBuf[8] = "20/20";
    char mpBuf[8] = "5/5";
    char depthBuf[8];
    std::snprintf(depthBuf, sizeof(depthBuf), "D%u", static_cast<unsigned>(depth));
    char lvlBuf[8] = "L1";
    game::FramePlan plan =
        planner.planFrame(layout, px, py, hpBuf, mpBuf, depthBuf, lvlBuf, "", "", tiles.data(), fogOfWar.data(),
                          monsters.data(), result.monsterCount, items.data(), result.itemCount, visible,
                          nullptr);
    (void)plan;
  }

  std::printf(
      "[fresh-floor-entry] %d floors driven through generate()+computeVisibility()+planFrame()\n", kFloors);
}

}  // namespace

int main() {
  testFreshFloorEntryAcrossSeedsAndDepths();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
