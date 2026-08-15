#pragma once

#include <GfxRenderer.h>

#include "Achievements.h"
#include "FrameDirtyPlanner.h"
#include "GameTheme.h"
#include "GameTypes.h"
#include "MappedInputManager.h"

class GameState;

// DirtyWindow/FramePlan now live in FrameDirtyPlanner.h (game namespace) so
// the pure planning logic -- and a host harness driving it -- has zero
// GfxRenderer/HAL dependency. Aliased here so existing call sites (draw(),
// planFrame()) don't need a `game::` prefix everywhere.
using DirtyWindow = game::DirtyWindow;
using FramePlan = game::FramePlan;

// Data shown on the blocking death/victory overlay (Phase 7 req 2/3). Populated
// by GameActivity right before it flips screenMode -- this struct has no
// knowledge of AchievementBus/GameState, it's just the rendered fields.
struct EndScreenData {
  char cause[32] = "";  // Death only; ignored for victory.
  uint8_t floor = 0;
  uint32_t turns = 0;
  uint16_t kills = 0;
  uint8_t level = 0;
  // Achievements unlocked THIS run. unlockedCount indexes into unlockedIds.
  game::AchievementId unlockedIds[static_cast<uint8_t>(game::AchievementId::Count)];
  uint8_t unlockedCount = 0;
};

// Renders the dungeon viewport, status bar, message log, and on-screen controls.
// The control area is also the touch control surface: a left-side d-pad (Up/Down/
// Left/Right, arranged in a 3-row cross) plus two bordered buttons on the right
// (Action, Menu) — see hitTestControls().
// Stateless — all data passed in or accessed via GameState singleton.
class GameRenderer {
 public:
  // Grid cell dimensions (pixels)
  static constexpr int CELL_W = 14;
  static constexpr int CELL_H = 20;

  // Screen layout (portrait 480x800)
  static constexpr int STATUS_Y = 2;
  static constexpr int STATUS_H = 26;
  static constexpr int VIEWPORT_Y = STATUS_H + 2;
  static constexpr int MESSAGE_H = 38;
  // Control area: 3 rows of the same 56px touch-target row height used by the old
  // hints bar ("comfortably above the 44x44 minimum recommended touch target"),
  // stacked to fit a 3-row-tall d-pad cross alongside two bordered action buttons.
  static constexpr int CONTROL_ROW_H = 56;
  static constexpr int CONTROLS_H = CONTROL_ROW_H * 3;

  // D-pad occupies the left half of the control area, laid out as 3 columns x 3 rows
  // (Up centered in the top row's middle column, Left/[decorative center]/Right in the
  // middle row, Down centered in the bottom row's middle column).
  static constexpr int DPAD_W = 168;         // Total d-pad width (3 equal columns)
  static constexpr int DPAD_COL_W = DPAD_W / 3;

  // Action/Menu bordered buttons occupy the right half of the control area, stacked
  // vertically (Action on top, Menu below), each spanning the remaining width.
  static constexpr int ACTION_MENU_BUTTON_COUNT = 2;

  // Computed at init
  int viewportW = 0;   // Pixels
  int viewportH = 0;   // Pixels
  int viewCols = 0;    // Grid columns
  int viewRows = 0;    // Grid rows
  int viewportEndY = 0;
  int messageY = 0;
  int controlsY = 0;
  int screenW = 0;
  int screenH = 0;
  int gridOffsetX = 0; // Left padding to center grid

  // Active sprite theme, re-read from CrossPointSettings once per draw() call
  // (never inside the per-cell loop) and held for the duration of that render
  // pass. Never null -- defaults to &game::kThemeDefault (all-nullptr, i.e.
  // pure glyph rendering) until the first draw() runs.
  const game::TileTheme* activeTheme = &game::kThemeDefault;

  // Owned by this renderer so the ghost-guard cadence is independent of every
  // other screen's own counter (see GfxRenderer::displayBufferGhostGuard).
  // Starts at 1 so the very first draw() call clears any residue left by
  // whatever screen was on-panel before the game was entered.
  int ghostGuardCounter = 1;

  void init(GfxRenderer& renderer);
  // Same layout math as init(), without touching a GfxRenderer -- lets a host
  // harness (no HAL, no real display) construct a GameRenderer and drive
  // planFrame() against known screen dimensions.
  void initForTest(int screenWidth, int screenHeight);

  // Draw the full game screen
  void draw(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar, const game::Monster* monsters,
            uint8_t monsterCount, const game::Item* items, uint8_t itemCount, const bool* visible);

  // Computes what draw() would refresh for this frame (dirty viewport cells +
  // status bar/message changes vs. the cached previous frame), without
  // drawing anything or touching a renderer. draw() calls this and then
  // executes the plan; a harness calls it directly to get real, checkable
  // numbers for a scripted walk. Shares this GameRenderer's tracker state, so
  // repeated calls behave exactly like repeated draw() calls would.
  FramePlan planFrame(const game::Tile* tiles, const uint8_t* fogOfWar, const game::Monster* monsters,
                      uint8_t monsterCount, const game::Item* items, uint8_t itemCount, const bool* visible);

  // Forces the next draw()/planFrame() to take the full-clear path. Call on
  // floor change or anything else that invalidates the cached frame beyond
  // what draw() already detects on its own (theme/viewport-origin changes are
  // detected automatically).
  void invalidateFrameCache() { planner_.invalidate(); }

  // Hit-tests a tap point against the control area (d-pad + Action/Menu buttons).
  // Returns true and sets outButton if the tap landed on a control, false otherwise.
  bool hitTestControls(int x, int y, MappedInputManager::Button& outButton) const;

  // Paints the blocking death/victory overlay on top of whatever's already on
  // the panel (Phase 7 req 2/3). Deliberately does NOT call clearScreen() --
  // Phase 8's dirty-rect rewrite lands on top of this, and a full repaint here
  // would fight it. Draws a self-contained bordered box centered on screen;
  // triggers its own FULL_REFRESH since this is new high-contrast content over
  // a stale buffer (ghost-guard cadence doesn't apply to a one-shot modal).
  void drawEndScreen(GfxRenderer& renderer, bool isVictory, const EndScreenData& data) const;

 private:
  void computeLayout(int screenWidth, int screenHeight);

  void drawStatusBar(GfxRenderer& renderer) const;
  void drawViewport(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar,
                    const game::Monster* monsters, uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                    const bool* visible) const;
  // Single choke point for tile/player/monster/item drawing. `sprite` is the
  // theme-resolved sprite for whatever occupies this cell (may be null, or
  // point at a Sprite2bpp with null `data` -- both mean "no art"). When no
  // sprite is available this falls back to the existing glyph drawText()
  // path unchanged; when a real sprite is available it's blitted via
  // drawSprite() instead. No branching on theme/sprite availability exists
  // anywhere else in this file.
  void drawCell(GfxRenderer& renderer, int screenX, int screenY, char glyph, const Sprite2bpp* sprite,
               bool isVisible, bool isExplored) const;
  // One viewport cell's worth of drawViewport's loop body, factored out so
  // both the full redraw (drawViewport, every cell) and Phase 8's partial
  // redraw (draw(), only cells inside a dirty bbox) share one implementation.
  void drawViewportCell(GfxRenderer& renderer, int viewX, int viewY, int row, int col, const game::Tile* tiles,
                        const uint8_t* fogOfWar, const game::Monster* monsters, uint8_t monsterCount,
                        const game::Item* items, uint8_t itemCount, const bool* visible) const;
  void drawMessages(GfxRenderer& renderer) const;
  void drawControls(GfxRenderer& renderer) const;

  // Player-centered viewport top-left in map coordinates, clamped so the
  // viewport never runs off the map edge. Thin forwarder to planner_ so
  // drawViewport()/draw() (which need this for rendering, not just planning)
  // don't have to build a PlannerLayout themselves every call site.
  void computeViewOrigin(int playerX, int playerY, int* outViewX, int* outViewY) const;

  // Builds the PlannerLayout FrameDirtyPlanner needs from this renderer's own
  // computeLayout() output -- the only place screen-layout fields cross into
  // planner_'s plain-data world.
  game::PlannerLayout buildPlannerLayout() const;

  // Formats the four status-bar fields into caller-owned buffers so
  // drawStatusBar() (rendering) and planFrame() (diffing, via planner_) always
  // compare/render identical text -- avoids the two ever drifting apart.
  void formatStatusBarText(char hpBuf[24], char mpBuf[24], char depthBuf[16], char lvlBuf[16]) const;

  // Owns the cached previous frame + all dirty-diff logic (see
  // FrameDirtyPlanner.h) -- wholly free of GfxRenderer/HAL, so a host harness
  // can drive one of these directly with zero display dependency.
  game::FrameDirtyPlanner planner_;
};
