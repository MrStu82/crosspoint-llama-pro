#pragma once
// Pure dirty-rect planning logic for Phase 8 (World Dungeon: Reclaim the
// frame). Deliberately has ZERO dependency on GfxRenderer/HAL/CrossPointSettings/
// I18n/fonts -- only GameTypes.h, GameState.h, GameTheme.h and
// DirtyRectTracker.h, all of which are already proven host-compilable (the
// game_save round-trip harness links GameState.cpp/GameSave.cpp directly).
// This is the seam a host harness hooks: build a FrameDirtyPlanner, feed it a
// PlannerLayout + scripted game state, and read real, checkable FramePlan
// results without any renderer/display involved. GameRenderer owns one of
// these for its dirty-rect bookkeeping (planFrame()), but its
// drawViewportCell() is a SEPARATE, independently hand-written
// implementation of this same cell-visual-priority logic -- it does not call
// through to computeCellVisual() below. Keep any change to the priority
// order or MONSTER_DEFS/item/tile lookups in sync in both places by hand
// until they're deduplicated (tracked as follow-up work, not yet done).

#include "DirtyRectTracker.h"
#include "GameTheme.h"
#include "GameTypes.h"

#include <cstddef>
#include <cstring>

namespace game {

// A screen-coordinate rectangle that changed and needs a `displayWindow()`
// refresh.
struct DirtyWindow {
  enum class Kind { Viewport, StatusBar, Messages, Notification };
  Kind kind = Kind::Viewport;
  int x = 0, y = 0, w = 0, h = 0;
};

// What draw() would refresh for a given frame. `fullClear` means "the whole
// screen" (first draw / floor change / theme change) -- a viewport re-center
// no longer forces this (see computeViewOrigin()/planFrame() below); it's
// just another cell diff against the (now stale) previous-origin cache --
// draw() takes the clearScreen() path for that case instead of unioning a
// full-screen rect through the same code as a small partial update.
struct FramePlan {
  bool fullClear = false;
  // Sized for 5, not 4: viewport bbox, status bar, message log are appended
  // here in FrameDirtyPlanner; the notification box is appended separately by
  // GameRenderer::planFrame() (this struct has zero dependency on
  // notification state, by design -- see file header). The two call sites
  // must stay in sync on this count by hand; if you add a window kind to
  // either one, bump this size to match.
  DirtyWindow windows[5];
  int windowCount = 0;

  int64_t totalDirtyArea() const {
    if (fullClear) return 0;  // caller should count the full-clear case separately
    int64_t area = 0;
    for (int i = 0; i < windowCount; i++) area += static_cast<int64_t>(windows[i].w) * windows[i].h;
    return area;
  }
};

// Screen-layout inputs the planner needs every call, computed once by
// GameRenderer::computeLayout() (or a harness's initForTest()-equivalent) and
// passed in as plain data -- keeps this file free of any layout-owning class.
struct PlannerLayout {
  int viewCols = 0;
  int viewRows = 0;
  int gridOffsetX = 0;
  int viewportY = 0;
  int cellW = 0;
  int cellH = 0;
  int screenW = 0;
  int statusH = 0;
  int messageY = 0;
  int messageH = 0;
};

class FrameDirtyPlanner {
 public:
  // Forces the next planFrame() call to report fullClear=true (first draw,
  // floor change, or anything else that invalidates the cache beyond what
  // planFrame() detects on its own).
  void invalidate() { tracker_.invalidate(); }

  // True player-centered viewport, clamped so it never runs off the map
  // edge. Restored (parent msg 4148) now that the viewport itself is much
  // smaller than when the Fix 1 dead-zone (message 3992) was added -- back
  // then EVERY viewOriginChanged forced planFrame() into a full-screen
  // fullClear (clearScreen() + displayBufferGhostGuard(), periodically
  // escalating to a 1720ms HALF_REFRESH). That cost is gone: planFrame()
  // below no longer treats an origin change as fullClear-worthy on its own
  // -- it re-diffs the viewport against the (now stale, previous-origin)
  // cache like any other frame, which naturally yields a single bounded
  // Viewport DirtyWindow pushed through the existing partial/union
  // displayWindow() path (Fix 1b) instead of a full-screen redraw. A pure
  // function of playerX/playerY/layout -- no dependency on the previous
  // origin, so there's no "holding" or "catching up" behavior left to reason
  // about.
  void computeViewOrigin(int playerX, int playerY, const PlannerLayout& layout, int* outViewX, int* outViewY) const {
    int viewX = playerX - layout.viewCols / 2;
    int viewY = playerY - layout.viewRows / 2;

    viewX = viewX < 0 ? 0 : (viewX > MAP_WIDTH - layout.viewCols ? MAP_WIDTH - layout.viewCols : viewX);
    viewY = viewY < 0 ? 0 : (viewY > MAP_HEIGHT - layout.viewRows ? MAP_HEIGHT - layout.viewRows : viewY);
    *outViewX = viewX;
    *outViewY = viewY;
  }

  // Pure description of what a single viewport cell looks like right now --
  // glyph + visibility state only, enough to diff against the previous frame.
  // Mirrors GameRenderer::drawViewportCell's player > monster > item > tile
  // priority. NOTE: uses itemGlyph(type) only, not (type, subtype) -- a
  // same-tile item swap that changes only subtype could theoretically be
  // missed by this diff. Not reachable under current game logic (items don't
  // mutate in place), and self-heals on the next real change or ghost-guard
  // full refresh.
  CellVisual computeCellVisual(int mapX, int mapY, const Tile* tiles, const uint8_t* fogOfWar,
                                const Monster* monsters, uint8_t monsterCount, const Item* items, uint8_t itemCount,
                                const bool* visible, int playerX, int playerY,
                                uint8_t indexedMonsterGlyph = 0, uint8_t indexedItemGlyph = 0,
                                bool useIndexedOccupants = false) const {
    int mapIdx = mapY * MAP_WIDTH + mapX;
    bool isExplored = fogIsExplored(fogOfWar, mapX, mapY);
    bool isVisible = visible[mapIdx];

    CellVisual cv;
    if (!isExplored && !isVisible) {
      return cv;  // default: glyph=' ', visState=0 (unseen)
    }

    char glyph = tileGlyph(tiles[mapIdx]);

    if (isVisible) {
      if (mapX == playerX && mapY == playerY) {
        glyph = '@';
      } else {
        if (useIndexedOccupants) {
          if (indexedMonsterGlyph != 0) {
            glyph = static_cast<char>(indexedMonsterGlyph);
          } else if (indexedItemGlyph != 0) {
            glyph = static_cast<char>(indexedItemGlyph);
          }
        } else {
          bool foundMonster = false;
          for (uint8_t m = 0; m < monsterCount; m++) {
            if (monsters[m].x == mapX && monsters[m].y == mapY && monsters[m].hp > 0) {
              glyph = MONSTER_DEFS[monsters[m].type].glyph;
              foundMonster = true;
              break;
            }
          }
          if (!foundMonster) {
            for (uint8_t i = 0; i < itemCount; i++) {
              if (items[i].x == mapX && items[i].y == mapY) {
                glyph = itemGlyph(items[i].type);
                break;
              }
            }
          }
        }
      }
    }

    cv.glyph = static_cast<uint8_t>(glyph);
    cv.visState = isVisible ? 2 : 1;
    return cv;
  }

  // Computes what draw() would refresh for this frame (dirty viewport cells +
  // status bar/message changes vs. the cached previous frame). Reads player
  // state via the passed accessors rather than touching GAME_STATE directly,
  // so a harness can drive this with a synthetic player/messages without
  // linking GameState.cpp at all if it doesn't need to.
  FramePlan planFrame(const PlannerLayout& layout, int playerX, int playerY, const char* hpBuf, const char* mpBuf,
                       const char* depthBuf, const char* lvlBuf, const char* hungerBuf, const char* msg0,
                       const char* msg1, const Tile* tiles, const uint8_t* fogOfWar, const Monster* monsters,
                       uint8_t monsterCount, const Item* items, uint8_t itemCount, const bool* visible,
                       const TileTheme* activeTheme) {
    FramePlan plan;

    int viewX = 0;
    int viewY = 0;
    computeViewOrigin(playerX, playerY, layout, &viewX, &viewY);

    const int viewportCellCount = layout.viewCols * layout.viewRows;
    memset(monsterGlyphs_, 0, static_cast<size_t>(viewportCellCount));
    memset(itemGlyphs_, 0, static_cast<size_t>(viewportCellCount));

    // Index occupants once per frame. Previously every viewport cell scanned
    // all monsters and then all items. Separate arrays preserve the existing
    // monster-over-item priority; first occupant wins, matching forward scan.
    for (uint8_t i = 0; i < monsterCount; i++) {
      if (monsters[i].hp == 0) continue;
      const int col = monsters[i].x - viewX;
      const int row = monsters[i].y - viewY;
      if (col < 0 || col >= layout.viewCols || row < 0 || row >= layout.viewRows) continue;
      const int index = row * layout.viewCols + col;
      if (monsterGlyphs_[index] == 0) monsterGlyphs_[index] = MONSTER_DEFS[monsters[i].type].glyph;
    }
    for (uint8_t i = 0; i < itemCount; i++) {
      const int col = items[i].x - viewX;
      const int row = items[i].y - viewY;
      if (col < 0 || col >= layout.viewCols || row < 0 || row >= layout.viewRows) continue;
      const int index = row * layout.viewCols + col;
      if (itemGlyphs_[index] == 0) itemGlyphs_[index] = itemGlyph(items[i].type);
    }

    const bool themeChanged = (activeTheme != lastTheme_);
    const bool needFullClear = !tracker_.hasSnapshot() || themeChanged;

    lastTheme_ = activeTheme;

    // viewCols/viewRows come from GameRenderer::computeLayout(), always well
    // within MAX_TRACK_COLS/ROWS (40x32) on any screen this renderer targets.
    // cells_/dirty_ are member storage (not locals) -- see class declaration --
    // so this frame's stack cost doesn't include their combined ~7.5KB.
    for (int row = 0; row < layout.viewRows; row++) {
      int mapY = viewY + row;
      for (int col = 0; col < layout.viewCols; col++) {
        int mapX = viewX + col;
        CellVisual cv;
        if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
          const int index = row * layout.viewCols + col;
          cv = computeCellVisual(mapX, mapY, tiles, fogOfWar, monsters, monsterCount,
                                 items, itemCount, visible, playerX, playerY,
                                 monsterGlyphs_[index], itemGlyphs_[index], true);
        }
        cells_[row * layout.viewCols + col] = cv;
      }
    }

    int dirtyCount = 0;
    tracker_.diffViewport(cells_, layout.viewCols, layout.viewRows, dirty_, MAX_DIRTY_CELLS, &dirtyCount);

    bool statusChanged = tracker_.statusBarChanged(hpBuf, mpBuf, depthBuf, lvlBuf, hungerBuf);
    bool messagesChangedFlag = tracker_.messagesChanged(msg0, msg1);

    if (needFullClear) {
      // Cache is now seeded (tracker_ calls above always adopt the new
      // snapshot) so the *next* frame can diff normally -- this frame itself
      // still takes the full-clear path.
      plan.fullClear = true;
      return plan;
    }

    if (dirtyCount > 0) {
      int rowMin = layout.viewRows;
      int rowMax = -1;
      int colMin = layout.viewCols;
      int colMax = -1;
      int cappedCount = dirtyCount < MAX_DIRTY_CELLS ? dirtyCount : MAX_DIRTY_CELLS;
      for (int i = 0; i < cappedCount; i++) {
        if (dirty_[i].row < rowMin) rowMin = dirty_[i].row;
        if (dirty_[i].row > rowMax) rowMax = dirty_[i].row;
        if (dirty_[i].col < colMin) colMin = dirty_[i].col;
        if (dirty_[i].col > colMax) colMax = dirty_[i].col;
      }
      DirtyWindow w;
      w.kind = DirtyWindow::Kind::Viewport;
      w.x = layout.gridOffsetX + colMin * layout.cellW;
      w.y = layout.viewportY + rowMin * layout.cellH;
      w.w = (colMax - colMin + 1) * layout.cellW;
      w.h = (rowMax - rowMin + 1) * layout.cellH;
      plan.windows[plan.windowCount++] = w;
    }

    if (statusChanged) {
      plan.windows[plan.windowCount++] =
          DirtyWindow{DirtyWindow::Kind::StatusBar, 0, 0, layout.screenW, layout.statusH};
    }

    if (messagesChangedFlag) {
      plan.windows[plan.windowCount++] =
          DirtyWindow{DirtyWindow::Kind::Messages, 0, layout.messageY, layout.screenW, layout.messageH};
    }

    return plan;
  }

 private:
  DirtyRectTracker tracker_;
  const TileTheme* lastTheme_ = nullptr;
  // Formerly locals in planFrame() (~2.5KB + ~5KB) -- the ActivityManagerRender
  // task's stack is a fixed 8192 bytes, and that combined 7.5KB left razor-thin
  // headroom for the rest of the render call chain, tripping ESP-IDF's stack
  // guard mid-way through dirty_'s zero-init loop. FrameDirtyPlanner is always a
  // heap-resident sub-object (see GameRenderer::planner_), so member storage
  // here is heap, not stack.
  CellVisual cells_[MAX_TRACK_COLS * MAX_TRACK_ROWS];
  DirtyCell dirty_[MAX_DIRTY_CELLS];
  uint8_t monsterGlyphs_[MAX_TRACK_COLS * MAX_TRACK_ROWS];
  uint8_t itemGlyphs_[MAX_TRACK_COLS * MAX_TRACK_ROWS];
};

}  // namespace game
