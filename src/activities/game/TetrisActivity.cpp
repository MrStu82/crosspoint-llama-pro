#include "TetrisActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// 7 tetrominoes, spawn orientation only -- other rotations are derived at
// runtime via TetrisActivity::rotateCW()/rotateCCW(), so there is exactly one
// shape literal per piece to get wrong instead of 28.
constexpr uint16_t kBaseShapes[7] = {
    0x0F00,  // I
    0x0660,  // O
    0x04E0,  // T
    0x06C0,  // S
    0x0C60,  // Z
    0x08E0,  // J
    0x02E0,  // L
};

constexpr int kLineScores[4] = {100, 300, 500, 800};
}  // namespace

bool TetrisActivity::cellSet(Shape shape, int row, int col) { return (shape >> (15 - (row * 4 + col))) & 1; }

TetrisActivity::Shape TetrisActivity::rotateCW(Shape shape) {
  Shape out = 0;
  for (int row = 0; row < kBoxSize; ++row) {
    for (int col = 0; col < kBoxSize; ++col) {
      if (cellSet(shape, row, col)) {
        const int newRow = col;
        const int newCol = kBoxSize - 1 - row;
        out |= static_cast<Shape>(1u << (15 - (newRow * 4 + newCol)));
      }
    }
  }
  return out;
}

TetrisActivity::Shape TetrisActivity::rotateCCW(Shape shape) {
  Shape out = 0;
  for (int row = 0; row < kBoxSize; ++row) {
    for (int col = 0; col < kBoxSize; ++col) {
      if (cellSet(shape, row, col)) {
        const int newRow = kBoxSize - 1 - col;
        const int newCol = row;
        out |= static_cast<Shape>(1u << (15 - (newRow * 4 + newCol)));
      }
    }
  }
  return out;
}

bool TetrisActivity::collides(Shape shape, int x, int y) const {
  for (int row = 0; row < kBoxSize; ++row) {
    for (int col = 0; col < kBoxSize; ++col) {
      if (!cellSet(shape, row, col)) continue;
      const int boardX = x + col;
      const int boardY = y + row;
      if (boardX < 0 || boardX >= kCols || boardY < 0 || boardY >= kRows) return true;
      if (board[boardY][boardX] != 0) return true;
    }
  }
  return false;
}

bool TetrisActivity::tryMove(int dx, int dy) {
  if (collides(currentShape, pieceX + dx, pieceY + dy)) return false;
  pieceX += dx;
  pieceY += dy;
  return true;
}

bool TetrisActivity::tryRotate(bool clockwise) {
  const Shape rotated = clockwise ? rotateCW(currentShape) : rotateCCW(currentShape);
  static constexpr int kKickOffsets[] = {0, -1, 1, -2, 2};
  for (int offset : kKickOffsets) {
    if (!collides(rotated, pieceX + offset, pieceY)) {
      currentShape = rotated;
      pieceX += offset;
      currentRotation = clockwise ? (currentRotation + 1) % 4 : (currentRotation + 3) % 4;
      return true;
    }
  }
  return false;
}

void TetrisActivity::hardDrop() {
  while (tryMove(0, 1)) {
  }
  lockPieceAndAdvance();
}

void TetrisActivity::holdSwap() {
  if (gameOver || holdUsed) return;

  if (holdType == kNoHold) {
    holdType = currentType;
    spawnPiece();
  } else {
    const int swapped = holdType;
    holdType = currentType;
    currentType = swapped;
    currentRotation = 0;
    currentShape = kBaseShapes[currentType];
    pieceX = (kCols - kBoxSize) / 2;
    pieceY = 0;
    if (collides(currentShape, pieceX, pieceY)) gameOver = true;
  }
  holdUsed = true;
}

int TetrisActivity::clearFullLines() {
  int cleared = 0;
  for (int row = kRows - 1; row >= 0; --row) {
    bool full = true;
    for (int col = 0; col < kCols; ++col) {
      if (board[row][col] == 0) {
        full = false;
        break;
      }
    }
    if (!full) continue;
    ++cleared;
    for (int r = row; r > 0; --r) {
      for (int col = 0; col < kCols; ++col) board[r][col] = board[r - 1][col];
    }
    for (int col = 0; col < kCols; ++col) board[0][col] = 0;
    ++row;  // re-check this row index, now holding the row shifted down into it
  }
  return cleared;
}

void TetrisActivity::spawnPiece() {
  currentType = nextType;
  nextType = static_cast<int>(random(7));
  currentRotation = 0;
  currentShape = kBaseShapes[currentType];
  pieceX = (kCols - kBoxSize) / 2;
  pieceY = 0;
  holdUsed = false;
  if (collides(currentShape, pieceX, pieceY)) gameOver = true;
}

void TetrisActivity::lockPieceAndAdvance() {
  for (int row = 0; row < kBoxSize; ++row) {
    for (int col = 0; col < kBoxSize; ++col) {
      if (!cellSet(currentShape, row, col)) continue;
      const int boardX = pieceX + col;
      const int boardY = pieceY + row;
      if (boardY >= 0 && boardY < kRows && boardX >= 0 && boardX < kCols) {
        board[boardY][boardX] = static_cast<uint8_t>(currentType + 1);
      }
    }
  }

  const int cleared = clearFullLines();
  if (cleared > 0) {
    linesCleared += cleared;
    score += kLineScores[cleared - 1] * (level + 1);
    level = linesCleared / 10;
    const int interval = 800 - level * 50;
    dropIntervalMs = static_cast<unsigned long>(interval < 100 ? 100 : interval);
  }

  spawnPiece();
}

void TetrisActivity::onEnter() {
  Activity::onEnter();
  for (auto& row : board) {
    for (auto& cell : row) cell = 0;
  }
  score = 0;
  linesCleared = 0;
  level = 0;
  dropIntervalMs = 800;
  gameOver = false;
  holdType = kNoHold;
  holdUsed = false;
  nextType = static_cast<int>(random(7));
  spawnPiece();
  lastDropMs = millis();
  requestUpdate();
}

void TetrisActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  if (gameOver) {
    if (mappedInput.wasReleased(Button::Confirm)) finish();
    return;
  }

  bool acted = false;

  // Secondary input: physical D-pad, same idiom as GameActivity::loop() /
  // Deep Mines. Kept alongside touch so d-pad play isn't a regression.
  if (mappedInput.wasReleased(Button::Left)) {
    tryMove(-1, 0);
    acted = true;
  } else if (mappedInput.wasReleased(Button::Right)) {
    tryMove(1, 0);
    acted = true;
  } else if (mappedInput.wasReleased(Button::Down)) {
    if (!tryMove(0, 1)) lockPieceAndAdvance();
    lastDropMs = millis();
    acted = true;
  } else if (mappedInput.wasReleased(Button::Up)) {
    tryRotate(true);
    acted = true;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    hardDrop();
    lastDropMs = millis();
    acted = true;
  }

  // Primary input: touch, per Stuart's spec -- swipe to move/drop, tap
  // left/right half to rotate, tap the bank slot to hold/swap.
  if (!acted) {
    switch (mappedInput.wasSwipe()) {
      case MappedInputManager::SwipeDir::Left:
        tryMove(-1, 0);
        acted = true;
        break;
      case MappedInputManager::SwipeDir::Right:
        tryMove(1, 0);
        acted = true;
        break;
      case MappedInputManager::SwipeDir::Down:
        if (!tryMove(0, 1)) lockPieceAndAdvance();
        lastDropMs = millis();
        acted = true;
        break;
      case MappedInputManager::SwipeDir::Up:
        hardDrop();
        lastDropMs = millis();
        acted = true;
        break;
      case MappedInputManager::SwipeDir::None:
        break;
    }
  }

  if (!acted) {
    if (mappedInput.wasTapInRect(bankRectX, bankRectY, bankRectWidth, bankRectHeight)) {
      holdSwap();
      acted = true;
    } else {
      int tx = 0, ty = 0;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        tryRotate(tx >= renderer.getScreenWidth() / 2);  // right half = CW, left half = CCW
        acted = true;
      }
    }
  }

  if (acted) requestUpdate();

  if (gameOver) {
    requestUpdate();
    return;
  }

  const unsigned long now = millis();
  if (now - lastDropMs >= dropIntervalMs) {
    lastDropMs = now;
    if (!tryMove(0, 1)) lockPieceAndAdvance();
    requestUpdate();
  }
}

void TetrisActivity::drawCell(int screenX, int screenY, int cellSize, bool filled) const {
  if (filled) {
    renderer.fillRect(screenX + 1, screenY + 1, cellSize - 2, cellSize - 2, true);
  }
}

void TetrisActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TETRIS_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const int sidePanelWidth = 120;
  const int padding = 8;
  const int boardAreaWidth = pageWidth - sidePanelWidth - padding * 3;

  const int cellSize = std::min(boardAreaWidth / kCols, contentHeight / kRows);
  const int boardWidth = cellSize * kCols;
  const int boardHeight = cellSize * kRows;
  const int boardLeft = padding;
  const int boardTop = contentTop + (contentHeight - boardHeight) / 2;

  renderer.drawRect(boardLeft, boardTop, boardWidth, boardHeight, true);

  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      if (board[row][col] != 0) {
        drawCell(boardLeft + col * cellSize, boardTop + row * cellSize, cellSize, true);
      }
    }
  }

  if (!gameOver) {
    for (int row = 0; row < kBoxSize; ++row) {
      for (int col = 0; col < kBoxSize; ++col) {
        if (!cellSet(currentShape, row, col)) continue;
        const int boardX = pieceX + col;
        const int boardY = pieceY + row;
        if (boardY < 0 || boardY >= kRows || boardX < 0 || boardX >= kCols) continue;
        drawCell(boardLeft + boardX * cellSize, boardTop + boardY * cellSize, cellSize, true);
      }
    }
  }

  const int panelLeft = boardLeft + boardWidth + padding * 2;
  int panelY = boardTop;
  const int previewCell = 16;

  // Bank (hold) slot -- also the touch hit-rect read back in loop().
  renderer.drawText(UI_12_FONT_ID, panelLeft, panelY, tr(STR_TETRIS_HOLD), true);
  panelY += 20;
  bankRectX = panelLeft;
  bankRectY = panelY;
  bankRectWidth = previewCell * kBoxSize;
  bankRectHeight = previewCell * kBoxSize;
  renderer.drawRect(bankRectX, bankRectY, bankRectWidth, bankRectHeight, true);
  if (holdType != kNoHold) {
    for (int row = 0; row < kBoxSize; ++row) {
      for (int col = 0; col < kBoxSize; ++col) {
        if (cellSet(kBaseShapes[holdType], row, col)) {
          drawCell(bankRectX + col * previewCell, bankRectY + row * previewCell, previewCell, true);
        }
      }
    }
  }
  panelY = bankRectY + previewCell * kBoxSize + 16;

  renderer.drawText(UI_12_FONT_ID, panelLeft, panelY, tr(STR_TETRIS_NEXT), true);
  panelY += 20;

  const int previewBoxTop = panelY;
  for (int row = 0; row < kBoxSize; ++row) {
    for (int col = 0; col < kBoxSize; ++col) {
      if (cellSet(kBaseShapes[nextType], row, col)) {
        drawCell(panelLeft + col * previewCell, previewBoxTop + row * previewCell, previewCell, true);
      }
    }
  }
  panelY = previewBoxTop + previewCell * kBoxSize + 16;

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%s: %d", tr(STR_TETRIS_SCORE), score);
  renderer.drawText(UI_12_FONT_ID, panelLeft, panelY, buf, true);
  panelY += 20;

  std::snprintf(buf, sizeof(buf), "%s: %d", tr(STR_TETRIS_LINES), linesCleared);
  renderer.drawText(UI_12_FONT_ID, panelLeft, panelY, buf, true);

  if (gameOver) {
    const int overlayY = boardTop + boardHeight / 2 - 12;
    renderer.drawCenteredText(UI_12_FONT_ID, overlayY, tr(STR_TETRIS_GAME_OVER), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
