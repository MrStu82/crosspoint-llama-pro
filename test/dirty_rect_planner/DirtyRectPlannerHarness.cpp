// Phase 8 ("World Dungeon: Reclaim the frame") host harness.
//
// Drives game::FrameDirtyPlanner -- the SAME translation unit GameRenderer
// delegates to (see GameRenderer.h/.cpp) -- through a scripted walk on
// synthetic map data, with zero GfxRenderer/HAL/display involved. This is a
// real link of the shipping planning code, not a transcription: per parent's
// standing instruction (msg 3632), "a transcribed formula proves the maths,
// never the code."
//
// Requirements being proven (parent msg 3630):
//   1. Cache the VIEWPORT, not the 80x50 map -- report sizeof.
//   2. (No scope change asserted here -- verified by inspection: GameRenderer
//      is the only caller of FrameDirtyPlanner, no other screen touches it.)
//   3. A NUMBER: per-step displayWindow call count + total dirty pixel area
//      for a scripted walk. Single-cell move -> small bounded rect. Floor
//      change -> full clear.
//
// Build:
//   g++ -std=c++20 -O2 -Wall -Wextra -I src/game \
//       test/dirty_rect_planner/DirtyRectPlannerHarness.cpp \
//       -o /tmp/dirty_rect_planner_harness
//
// No stub-override layer needed (unlike the Phase 7 game_save harness) --
// FrameDirtyPlanner.h is transitively pure (DirtyRectTracker.h, GameTheme.h,
// GameTypes.h, Sprite2bpp.h, sprites/world_dungeon_default_sprites.h are all
// already host-compilable with only <cstdint>/<cstdio>/<cstring>).

#include "FrameDirtyPlanner.h"

#include <cstdio>
#include <cstring>

using namespace game;

namespace {

int failures = 0;

#define CHECK(cond, ...)                     \
  do {                                        \
    if (!(cond)) {                            \
      failures++;                             \
      std::printf("FAIL: " __VA_ARGS__);      \
      std::printf("  (%s:%d)\n", __FILE__, __LINE__); \
    }                                         \
  } while (0)

// Synthetic 80x50 map: walled border, alternating Floor/Wall interior
// columns (a uniform-floor interior would make step 5's origin-shift case
// degenerate -- see step 5's comment below -- since a viewport that
// re-centers together with the player leaves the player's own screen
// position unchanged frame-to-frame, and every other on-screen cell would
// show the same floor glyph before and after the shift regardless of which
// map columns actually moved into/out of view). Column-alternating Wall/
// Floor gives each interior column a visually distinct glyph ('#' vs '.'),
// so shifting the viewport by one column genuinely changes what's rendered
// at most on-screen positions, exercising the diff the way a real dungeon
// (never uniform) would.
void buildMap(Tile tiles[MAP_SIZE]) {
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      bool border = (x == 0 || y == 0 || x == MAP_WIDTH - 1 || y == MAP_HEIGHT - 1);
      tiles[y * MAP_WIDTH + x] = border ? Tile::Wall : ((x % 2 == 0) ? Tile::Floor : Tile::Wall);
    }
  }
}

// Real screen layout constants (GameRenderer.h), reproduced here as plain
// data since this harness has no GameRenderer/GfxRenderer to compute them.
PlannerLayout buildLayout() {
  PlannerLayout layout;
  layout.viewCols = 34;
  layout.viewRows = 28;
  layout.cellW = 14;
  layout.cellH = 20;
  layout.screenW = 480;
  layout.gridOffsetX = (480 - 34 * 14) / 2;  // = 2
  layout.viewportY = 28;                      // STATUS_H(26) + 2
  layout.statusH = 26;
  layout.messageY = 762;                      // arbitrary, below viewport+controls
  layout.messageH = 38;
  return layout;
}

struct StepResult {
  bool fullClear = false;
  int windowCount = 0;
  int64_t totalArea = 0;
};

StepResult runStep(FrameDirtyPlanner& planner, const PlannerLayout& layout, int playerX, int playerY,
                    const Tile* tiles, const uint8_t* fog, const TileTheme* theme) {
  bool visible[MAP_SIZE];
  for (int i = 0; i < MAP_SIZE; i++) visible[i] = false;
  // Mark the current viewport's cells visible -- mirrors what the real game
  // does once fog-of-war/line-of-sight resolves a frame's visible set.
  int viewX = 0, viewY = 0;
  planner.computeViewOrigin(playerX, playerY, layout, &viewX, &viewY);
  for (int row = 0; row < layout.viewRows; row++) {
    for (int col = 0; col < layout.viewCols; col++) {
      int mapX = viewX + col;
      int mapY = viewY + row;
      if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
        visible[mapY * MAP_WIDTH + mapX] = true;
      }
    }
  }

  char hp[24], mp[24], depth[16], lvl[16], hunger[16];
  std::snprintf(hp, sizeof(hp), "HP 20/20");
  std::snprintf(mp, sizeof(mp), "MP 5/5");
  std::snprintf(depth, sizeof(depth), "D1");
  std::snprintf(lvl, sizeof(lvl), "L1");
  std::snprintf(hunger, sizeof(hunger), "Fed");
  const char* msg0 = "Welcome to World Dungeon.";
  const char* msg1 = "";

  FramePlan plan = planner.planFrame(layout, playerX, playerY, hp, mp, depth, lvl, hunger, msg0, msg1, tiles, fog,
                                     nullptr, 0, nullptr, 0, visible, theme);

  StepResult r;
  r.fullClear = plan.fullClear;
  r.windowCount = plan.fullClear ? 1 : plan.windowCount;  // full clear = one displayBuffer() call, not displayWindow()
  r.totalArea = plan.fullClear ? static_cast<int64_t>(layout.screenW) * 800 : plan.totalDirtyArea();
  return r;
}

}  // namespace

int main() {
  std::printf("=== Phase 8 dirty-rect planner harness ===\n\n");

  // --- Requirement 1: sizeof the cached snapshot, not the 80x50 map ---
  std::printf("sizeof(DirtyRectTracker)   = %zu bytes\n", sizeof(DirtyRectTracker));
  std::printf("sizeof(FrameDirtyPlanner)  = %zu bytes\n", sizeof(FrameDirtyPlanner));
  std::printf("sizeof(Tile)*MAP_SIZE      = %zu bytes (the FULL map, for comparison -- NOT what's cached)\n",
              sizeof(Tile) * MAP_SIZE);
  CHECK(sizeof(DirtyRectTracker) < 4096, "DirtyRectTracker must stay under a couple of KB, got %zu\n",
        sizeof(DirtyRectTracker));
  std::printf("\n");

  // --- Requirement 3: scripted walk, per-step numbers ---
  static Tile tiles[MAP_SIZE];
  static uint8_t fog[FOG_SIZE] = {};
  buildMap(tiles);
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) fogSetExplored(fog, x, y);
  }

  PlannerLayout layout = buildLayout();
  FrameDirtyPlanner planner;

  // Player starts in the top-left corner region (x=5,y=5), well inside the
  // clamp band (viewCols/2=17, viewRows/2=14) -- viewOrigin is pinned at
  // (0,0) for the whole walk below, so player movement diffs a STATIONARY
  // viewport instead of re-centering (and full-clearing) every step. This is
  // the real steady-state case for most on-screen movement near a map edge;
  // interior movement re-centers every step by design (computeViewOrigin
  // tracks the player 1:1 until clamped) and is exercised separately below.
  int px = 5, py = 5;

  std::printf("Step 1: first frame (no snapshot yet)\n");
  StepResult s1 = runStep(planner, layout, px, py, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s1.fullClear, s1.windowCount,
              (long long)s1.totalArea);
  CHECK(s1.fullClear, "step 1 (first frame) must be a full clear\n");

  std::printf("Step 2: single-cell move (5,5)->(6,5), viewport pinned by edge clamp\n");
  px = 6;
  StepResult s2 = runStep(planner, layout, px, py, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s2.fullClear, s2.windowCount,
              (long long)s2.totalArea);
  CHECK(!s2.fullClear, "step 2 (single-cell move, clamped viewport) should NOT be a full clear\n");
  CHECK(s2.windowCount >= 1 && s2.windowCount <= 3, "step 2 windowCount should be small (viewport+status), got %d\n",
        s2.windowCount);
  // Two cells changed (old player cell reverts to floor glyph, new cell
  // becomes '@') -> bbox is at most 2 cells wide, well under the full 34x28
  // viewport (34*20*14*20 = 9520px). Bound generously at 1/4 of the viewport
  // area to allow for the status-bar text also legitimately changing (it
  // doesn't, here, since hp/mp/depth/lvl are constant across this walk).
  CHECK(s2.totalArea > 0 && s2.totalArea < (static_cast<int64_t>(layout.viewCols) * layout.cellW * layout.viewRows *
                                            layout.cellH) / 4,
        "step 2 dirty area should be a small bounded rect, got %lld\n", (long long)s2.totalArea);

  std::printf("Step 3: another single-cell move (6,5)->(7,5)\n");
  px = 7;
  StepResult s3 = runStep(planner, layout, px, py, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s3.fullClear, s3.windowCount,
              (long long)s3.totalArea);
  CHECK(!s3.fullClear, "step 3 (single-cell move, clamped viewport) should NOT be a full clear\n");
  CHECK(s3.totalArea > 0 && s3.totalArea < (static_cast<int64_t>(layout.viewCols) * layout.cellW * layout.viewRows *
                                            layout.cellH) / 4,
        "step 3 dirty area should be a small bounded rect, got %lld\n", (long long)s3.totalArea);

  std::printf("Step 4: floor change (invalidate(), simulating a level transition)\n");
  planner.invalidate();
  StepResult s4 = runStep(planner, layout, px, py, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s4.fullClear, s4.windowCount,
              (long long)s4.totalArea);
  CHECK(s4.fullClear, "step 4 (floor change / invalidate) must be a full clear\n");

  std::printf("Step 5: interior move, no invalidate -- (40,25)->(41,25), unclamped viewport re-centers\n");
  std::printf("  (uses a SEPARATE planner instance -- this is an isolated demo, not part of the corner walk's\n");
  std::printf("   cache lineage; step 8 below resumes the corner-walk planner from step 3/4's state.)\n");
  std::printf("  Restored true centering (parent msg 4148): an origin change no longer forces fullClear -- it\n");
  std::printf("  re-diffs against the stale previous-origin cache and goes through the same bounded partial\n");
  std::printf("  displayWindow() path as any other frame, just with a larger (near-full-viewport) dirty rect.\n");
  FrameDirtyPlanner interiorPlanner;
  StepResult s5a = runStep(interiorPlanner, layout, 40, 25, tiles, fog, &kThemeDefault);
  std::printf("  (seed frame at 40,25) fullClear=%d\n", s5a.fullClear);
  StepResult s5b = runStep(interiorPlanner, layout, 41, 25, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s5b.fullClear, s5b.windowCount,
              (long long)s5b.totalArea);
  CHECK(!s5b.fullClear,
        "step 5 (single-cell move in the map interior) must NOT full-clear -- restored centering routes an "
        "origin change through the bounded partial path instead\n");
  CHECK(s5b.windowCount >= 1, "step 5 (interior re-center) should still produce at least one displayWindow(), got %d\n",
        s5b.windowCount);
  int64_t fullScreenArea = static_cast<int64_t>(layout.screenW) * 800;
  int64_t viewportArea =
      static_cast<int64_t>(layout.viewCols) * layout.cellW * layout.viewRows * layout.cellH;
  CHECK(s5b.totalArea > 0 && s5b.totalArea <= viewportArea,
        "step 5 dirty area should be bounded by the viewport, not the full screen, got %lld (viewport=%lld, "
        "screen=%lld)\n",
        (long long)s5b.totalArea, (long long)viewportArea, (long long)fullScreenArea);
  CHECK(viewportArea < fullScreenArea,
        "sanity: viewport area must be meaningfully smaller than full screen for this fix to matter, got "
        "viewport=%lld screen=%lld\n",
        (long long)viewportArea, (long long)fullScreenArea);

  std::printf("\nStep 7: computeViewOrigin pins true centering, no dead-zone lag, edge-clamped\n");
  {
    FrameDirtyPlanner pinPlanner;
    int vx = 0, vy = 0;

    // Interior position, far from every edge: origin must be exactly
    // centered on the player -- a pure function of position, no held/lagged
    // state from a prior call.
    pinPlanner.computeViewOrigin(40, 25, layout, &vx, &vy);
    CHECK(vx == 40 - layout.viewCols / 2, "interior computeViewOrigin viewX should be exactly centered, got %d\n",
          vx);
    CHECK(vy == 25 - layout.viewRows / 2, "interior computeViewOrigin viewY should be exactly centered, got %d\n",
          vy);

    // One step over: with the old dead-zone this would NOT move the
    // viewport (still inside the margin); true centering must track the
    // player 1:1, every step, no lag.
    int vx2 = 0, vy2 = 0;
    pinPlanner.computeViewOrigin(41, 25, layout, &vx2, &vy2);
    CHECK(vx2 == vx + 1, "computeViewOrigin must track the player 1:1 with no dead-zone hold, got delta %d\n",
          vx2 - vx);
    CHECK(vy2 == vy, "computeViewOrigin viewY should not move on a pure X step, got %d\n", vy2);

    // Top-left corner: clamp kicks in, origin pins at (0,0) instead of going
    // negative -- player goes off-centre at the map edge, never shows void.
    int vx3 = 0, vy3 = 0;
    pinPlanner.computeViewOrigin(0, 0, layout, &vx3, &vy3);
    CHECK(vx3 == 0, "computeViewOrigin should clamp viewX to 0 at the top-left corner, got %d\n", vx3);
    CHECK(vy3 == 0, "computeViewOrigin should clamp viewY to 0 at the top-left corner, got %d\n", vy3);

    // Bottom-right corner: clamp pins at MAP_WIDTH/HEIGHT - view size.
    int vx4 = 0, vy4 = 0;
    pinPlanner.computeViewOrigin(MAP_WIDTH - 1, MAP_HEIGHT - 1, layout, &vx4, &vy4);
    CHECK(vx4 == MAP_WIDTH - layout.viewCols,
          "computeViewOrigin should clamp viewX at the bottom-right corner, got %d (want %d)\n", vx4,
          MAP_WIDTH - layout.viewCols);
    CHECK(vy4 == MAP_HEIGHT - layout.viewRows,
          "computeViewOrigin should clamp viewY at the bottom-right corner, got %d (want %d)\n", vy4,
          MAP_HEIGHT - layout.viewRows);
  }

  std::printf("\nStep 8: repeat step 4's frame with fully unchanged state -- no-op frame\n");
  StepResult s6 = runStep(planner, layout, px, py, tiles, fog, &kThemeDefault);
  std::printf("  fullClear=%d windowCount=%d totalArea=%lld\n", s6.fullClear, s6.windowCount,
              (long long)s6.totalArea);
  CHECK(!s6.fullClear, "step 8 (nothing changed) should not be a full clear\n");
  CHECK(s6.windowCount == 0, "step 8 (nothing changed) should produce zero displayWindow calls, got %d\n",
        s6.windowCount);
  CHECK(s6.totalArea == 0, "step 8 (nothing changed) should have zero dirty area, got %lld\n",
        (long long)s6.totalArea);

  std::printf("\n=== %s (%d failure%s) ===\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
  return failures;
}
