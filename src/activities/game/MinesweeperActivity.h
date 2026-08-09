#pragma once

#include <cstdint>
#include <vector>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

class MappedInputManager;

// Standard Minesweeper. Tap a hidden cell to reveal it, long-press a hidden
// cell to toggle a flag on it. Grid dimensions are chosen for legible
// numerals (target ~cellPx-sized cells derived from the panel's own
// resolution), not for maximum cell density. Mine placement is deferred
// until the first reveal so that tap is guaranteed safe; revealing a
// zero-adjacency cell flood-fills its connected zero region iteratively (no
// recursion, per the stack-budget rule). No engine/shared abstraction with
// the other games -- deliberately kept flat (YAGNI), matching Solitaire.
class MinesweeperActivity final : public Activity {
 public:
  explicit MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Minesweeper", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  struct Cell {
    bool mine = false;
    bool revealed = false;
    bool flagged = false;
    uint8_t adjacent = 0;  // count of adjacent mines, valid once mines are placed
  };

  enum class GameState : uint8_t { Playing, Won, Lost };

  static constexpr int kMinCols = 8;
  static constexpr int kMaxCols = 14;
  static constexpr int kMinRows = 8;
  static constexpr int kMaxRows = 16;
  static constexpr int kTargetCellPx = 42;
  static constexpr int kMineDensityPercent = 16;
  static constexpr uint32_t kLongPressMs = 500;
  static constexpr int kTouchMaxDist = 20;

  int cols = 0;
  int rows = 0;
  int mineCount = 0;
  int flagCount = 0;
  bool firstRevealDone = false;
  GameState state = GameState::Playing;
  std::vector<Cell> cells;

  // Touch/long-press tracking.
  bool touchActive = false;
  bool longPressFired = false;
  uint32_t touchStartMs = 0;
  int touchStartX = 0;
  int touchStartY = 0;

  // Hit-rects/geometry, recomputed each render() and read back by loop()'s touch handling.
  Rect gridRect;
  Rect newGameRect;
  int cellPx = 0;

  void newGame();
  void placeMines(int avoidIdx);
  void floodReveal(int startIdx);
  void revealCell(int idx);
  void toggleFlag(int idx);
  bool checkWin() const;
  int cellIndexAt(int tx, int ty) const;
  void handleTap(int tx, int ty);
  void handleLongPress(int tx, int ty);

  void drawCell(int r, int c) const;
};
