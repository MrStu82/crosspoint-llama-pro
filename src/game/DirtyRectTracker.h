#pragma once
// Pure diff logic backing GameRenderer's dirty-rect rendering (Phase 8). No
// GfxRenderer/HAL dependency on purpose -- a host harness can drive this with
// synthetic frames and get real, checkable numbers without a display.
//
// Deliberately caches the VIEWPORT (currently-visible cells), not the whole
// map -- that's the only thing ever on glass at once, and keeping the
// snapshot small matters on this device's memory budget.

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace game {

// Generous upper bound on viewport dimensions (actual viewCols/viewRows are
// computed at runtime from screen size / cell size and are smaller -- 34x28
// on the real 480x800 panel). Sized so the cached snapshot comfortably clears
// a couple of KB: 40*32 * 2 bytes = 2560 bytes.
constexpr int MAX_TRACK_COLS = 40;
constexpr int MAX_TRACK_ROWS = 32;

// Minimal description of what a viewport cell looks like on screen -- enough
// to detect "this cell needs to be redrawn," not enough to actually draw it
// (no sprite pointers/theme data, so this stays POD and host-testable).
struct CellVisual {
  uint8_t glyph = ' ';
  uint8_t visState = 0;  // 0 = unseen (blank), 1 = explored (dim/remembered), 2 = visible

  bool operator==(const CellVisual& o) const { return glyph == o.glyph && visState == o.visState; }
  bool operator!=(const CellVisual& o) const { return !(*this == o); }
};

// A single row/col coordinate of a cell found to differ from the cached frame.
struct DirtyCell {
  int16_t row = 0;
  int16_t col = 0;
};

constexpr int MAX_DIRTY_CELLS = MAX_TRACK_COLS * MAX_TRACK_ROWS;

// Tracks the last-drawn state of the game viewport + status bar + message log
// so GameRenderer::draw() can redraw (and displayWindow-refresh) only the
// regions that actually changed, instead of a full clearScreen() every step.
class DirtyRectTracker {
 public:
  // Forces the next diff to report "everything dirty" (used on first draw,
  // floor change, viewport re-center, or a theme change -- anything where a
  // full clear is cheaper/safer than reasoning about a diff).
  void invalidate() { valid_ = false; }

  bool hasSnapshot() const { return valid_; }

  // Diffs `cells` (row-major, cols x rows) against the cached snapshot.
  // Writes up to maxOut entries into outDirty[], sets *outDirtyCount to the
  // true number of changed cells (which may exceed maxOut -- callers sizing
  // maxOut at MAX_DIRTY_CELLS never truncate). Always adopts `cells` as the
  // new cached snapshot before returning, whether or not the tracker had a
  // valid prior snapshot.
  void diffViewport(const CellVisual* cells, int cols, int rows, DirtyCell* outDirty, int maxOut,
                     int* outDirtyCount) {
    *outDirtyCount = 0;
    const bool hadSnapshot = valid_ && cols == cachedCols_ && rows == cachedRows_;

    for (int row = 0; row < rows; row++) {
      for (int col = 0; col < cols; col++) {
        const CellVisual& next = cells[row * cols + col];
        const bool changed = !hadSnapshot || cells_[row * cols + col] != next;
        if (changed) {
          if (*outDirtyCount < maxOut) {
            outDirty[*outDirtyCount] = DirtyCell{static_cast<int16_t>(row), static_cast<int16_t>(col)};
          }
          (*outDirtyCount)++;
        }
        cells_[row * cols + col] = next;
      }
    }

    cachedCols_ = cols;
    cachedRows_ = rows;
    valid_ = true;
  }

  // Status bar / message text diffing -- simple string-changed checks. Each
  // call adopts the passed strings as the new cached value.
  bool statusBarChanged(const char* hp, const char* mp, const char* depth, const char* lvl, const char* hunger) {
    bool changed = strcmp(hp_, hp) != 0 || strcmp(mp_, mp) != 0 || strcmp(depth_, depth) != 0 ||
                   strcmp(lvl_, lvl) != 0 || strcmp(hunger_, hunger) != 0;
    snprintf(hp_, sizeof(hp_), "%s", hp);
    snprintf(mp_, sizeof(mp_), "%s", mp);
    snprintf(depth_, sizeof(depth_), "%s", depth);
    snprintf(lvl_, sizeof(lvl_), "%s", lvl);
    snprintf(hunger_, sizeof(hunger_), "%s", hunger);
    return changed;
  }

  bool messagesChanged(const char* msg0, const char* msg1) {
    bool changed = strcmp(msg0_, msg0) != 0 || strcmp(msg1_, msg1) != 0;
    snprintf(msg0_, sizeof(msg0_), "%s", msg0);
    snprintf(msg1_, sizeof(msg1_), "%s", msg1);
    return changed;
  }

 private:
  bool valid_ = false;
  int cachedCols_ = 0;
  int cachedRows_ = 0;
  CellVisual cells_[MAX_TRACK_COLS * MAX_TRACK_ROWS];
  char hp_[24] = "";
  char mp_[24] = "";
  char depth_[16] = "";
  char lvl_[16] = "";
  char hunger_[16] = "";
  char msg0_[160] = "";
  char msg1_[160] = "";
};

}  // namespace game
