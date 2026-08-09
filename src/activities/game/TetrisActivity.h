#pragma once

#include <cstdint>

#include "activities/Activity.h"

class MappedInputManager;

// Small self-contained Tetris implementation: fixed 10-wide/20-tall well, one
// next-piece preview, hold/swap bank slot, scoring from cleared lines only.
// Touch (swipe left/right/down/up, tap-half-to-rotate, tap-bank-to-hold) is
// the primary input per spec; the physical D-pad chain (reusing the exact
// idiom from GameActivity::loop()) is kept as a secondary path. No engine or
// shared abstraction with the other games -- deliberately kept flat (YAGNI).
class TetrisActivity final : public Activity {
 public:
  explicit TetrisActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tetris", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  static constexpr int kCols = 10;
  static constexpr int kRows = 20;
  static constexpr int kBoxSize = 4;  // pieces are stored in a 4x4 bounding box
  static constexpr int kNoHold = -1;

  using Shape = uint16_t;  // 16 bits, bit15 = (row0,col0) .. bit0 = (row3,col3)

  uint8_t board[kRows][kCols] = {};  // 0 = empty, else piece type + 1

  int currentType = 0;
  int currentRotation = 0;
  Shape currentShape = 0;
  int pieceX = 0;
  int pieceY = 0;
  int nextType = 0;

  int holdType = kNoHold;
  bool holdUsed = false;  // one hold/swap per piece, standard Tetris convention

  unsigned long lastDropMs = 0;
  unsigned long dropIntervalMs = 800;

  int score = 0;
  int linesCleared = 0;
  int level = 0;
  bool gameOver = false;

  // Bank (hold) slot hit-rect, recomputed each render() and read back by loop()'s touch handling.
  int bankRectX = 0;
  int bankRectY = 0;
  int bankRectWidth = 0;
  int bankRectHeight = 0;

  static bool cellSet(Shape shape, int row, int col);
  static Shape rotateCW(Shape shape);
  static Shape rotateCCW(Shape shape);

  bool collides(Shape shape, int x, int y) const;
  bool tryMove(int dx, int dy);
  bool tryRotate(bool clockwise);
  void hardDrop();
  void holdSwap();
  void lockPieceAndAdvance();
  void spawnPiece();
  int clearFullLines();
  void drawCell(int screenX, int screenY, int cellSize, bool filled) const;
};
