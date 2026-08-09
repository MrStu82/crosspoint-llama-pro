#pragma once

#include <cstdint>

#include "activities/Activity.h"

class MappedInputManager;

// Small self-contained Tetris implementation: fixed 10-wide/20-tall well, one
// next-piece preview, no hold, scoring from cleared lines only. No engine or
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

  using Shape = uint16_t;  // 16 bits, bit15 = (row0,col0) .. bit0 = (row3,col3)

  uint8_t board[kRows][kCols] = {};  // 0 = empty, else piece type + 1

  int currentType = 0;
  int currentRotation = 0;
  Shape currentShape = 0;
  int pieceX = 0;
  int pieceY = 0;
  int nextType = 0;

  unsigned long lastDropMs = 0;
  unsigned long dropIntervalMs = 800;

  int score = 0;
  int linesCleared = 0;
  int level = 0;
  bool gameOver = false;

  static bool cellSet(Shape shape, int row, int col);
  static Shape rotateCW(Shape shape);

  bool collides(Shape shape, int x, int y) const;
  bool tryMove(int dx, int dy);
  bool tryRotate();
  void hardDrop();
  void lockPieceAndAdvance();
  void spawnPiece();
  int clearFullLines();
  void drawCell(int screenX, int screenY, int cellSize, bool filled) const;
};
