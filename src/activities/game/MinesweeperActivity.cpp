#include "MinesweeperActivity.h"

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
bool rectContains(const Rect& r, int x, int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
}  // namespace

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  newGame();
  requestUpdate();
}

void MinesweeperActivity::newGame() {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int statusH = 30;
  const int gap = 6;
  const int marginX = 6;
  const int availW = pageWidth - 2 * marginX;
  const int availH = pageHeight - contentTop - statusH - gap - metrics.buttonHintsHeight - metrics.verticalSpacing;

  cols = std::max(kMinCols, std::min(kMaxCols, availW / kTargetCellPx));
  rows = std::max(kMinRows, std::min(kMaxRows, availH / kTargetCellPx));

  cells.assign(static_cast<size_t>(cols) * rows, Cell{});
  mineCount = std::max(1, cols * rows * kMineDensityPercent / 100);
  flagCount = 0;
  firstRevealDone = false;
  state = GameState::Playing;
  touchActive = false;
  longPressFired = false;
  forceFullRefresh = true;
}

void MinesweeperActivity::placeMines(int avoidIdx) {
  std::vector<int> candidates;
  candidates.reserve(cells.size());
  for (int i = 0; i < static_cast<int>(cells.size()); i++) {
    if (i != avoidIdx) candidates.push_back(i);
  }
  for (int i = static_cast<int>(candidates.size()) - 1; i > 0; i--) {
    int j = static_cast<int>(random(i + 1));
    std::swap(candidates[i], candidates[j]);
  }
  int toPlace = std::min(mineCount, static_cast<int>(candidates.size()));
  for (int i = 0; i < toPlace; i++) {
    cells[candidates[i]].mine = true;
  }
  mineCount = toPlace;

  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      int idx = r * cols + c;
      if (cells[idx].mine) continue;
      int cnt = 0;
      for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
          if (dr == 0 && dc == 0) continue;
          int nr = r + dr;
          int nc = c + dc;
          if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
          if (cells[nr * cols + nc].mine) cnt++;
        }
      }
      cells[idx].adjacent = static_cast<uint8_t>(cnt);
    }
  }
  firstRevealDone = true;
}

void MinesweeperActivity::floodReveal(int startIdx) {
  std::vector<int> stack;
  stack.reserve(cells.size());
  stack.push_back(startIdx);
  while (!stack.empty()) {
    int idx = stack.back();
    stack.pop_back();
    Cell& cell = cells[idx];
    if (cell.revealed || cell.flagged) continue;
    cell.revealed = true;
    if (cell.adjacent != 0 || cell.mine) continue;
    int r = idx / cols;
    int c = idx % cols;
    for (int dr = -1; dr <= 1; dr++) {
      for (int dc = -1; dc <= 1; dc++) {
        if (dr == 0 && dc == 0) continue;
        int nr = r + dr;
        int nc = c + dc;
        if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
        int nIdx = nr * cols + nc;
        if (!cells[nIdx].revealed && !cells[nIdx].flagged) stack.push_back(nIdx);
      }
    }
  }
}

void MinesweeperActivity::revealCell(int idx) {
  Cell& cell = cells[idx];
  if (cell.revealed || cell.flagged) return;
  if (cell.mine) {
    cell.revealed = true;
    state = GameState::Lost;
    for (auto& c : cells) {
      if (c.mine) c.revealed = true;
    }
    return;
  }
  if (cell.adjacent == 0) {
    floodReveal(idx);
  } else {
    cell.revealed = true;
  }
  if (checkWin()) {
    state = GameState::Won;
  }
}

void MinesweeperActivity::toggleFlag(int idx) {
  Cell& cell = cells[idx];
  if (cell.revealed) return;
  cell.flagged = !cell.flagged;
  flagCount += cell.flagged ? 1 : -1;
}

bool MinesweeperActivity::checkWin() const {
  for (const auto& c : cells) {
    if (!c.mine && !c.revealed) return false;
  }
  return true;
}

int MinesweeperActivity::cellIndexAt(int tx, int ty) const {
  if (!rectContains(gridRect, tx, ty) || cellPx <= 0) return -1;
  int c = (tx - gridRect.x) / cellPx;
  int r = (ty - gridRect.y) / cellPx;
  if (c < 0 || c >= cols || r < 0 || r >= rows) return -1;
  return r * cols + c;
}

void MinesweeperActivity::handleTap(int tx, int ty) {
  int idx = cellIndexAt(tx, ty);
  if (idx < 0) return;
  if (!firstRevealDone) {
    placeMines(idx);
  }
  revealCell(idx);
}

void MinesweeperActivity::handleLongPress(int tx, int ty) {
  int idx = cellIndexAt(tx, ty);
  if (idx < 0) return;
  toggleFlag(idx);
}

void MinesweeperActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  int hx = 0;
  int hy = 0;
  bool held = mappedInput.isScreenTouchHeld(hx, hy);
  if (held) {
    if (!touchActive) {
      touchActive = true;
      longPressFired = false;
      touchStartMs = millis();
      touchStartX = hx;
      touchStartY = hy;
    } else if (!longPressFired && state == GameState::Playing) {
      int dx = hx - touchStartX;
      int dy = hy - touchStartY;
      if (dx * dx + dy * dy <= kTouchMaxDist * kTouchMaxDist && millis() - touchStartMs >= kLongPressMs) {
        longPressFired = true;
        handleLongPress(touchStartX, touchStartY);
        forceFullRefresh = true;
        requestUpdate();
        return;
      }
    }
  } else {
    touchActive = false;
  }

  int tx = 0;
  int ty = 0;
  bool tapped = mappedInput.wasScreenTapped(tx, ty);
  if (!tapped) return;

  if (rectContains(newGameRect, tx, ty) || mappedInput.wasPressed(Button::Confirm)) {
    newGame();
    requestUpdate();
    return;
  }

  bool wasLongPress = longPressFired;
  longPressFired = false;
  if (wasLongPress || state != GameState::Playing) return;

  handleTap(tx, ty);
  forceFullRefresh = true;
  requestUpdate();
}

void MinesweeperActivity::drawCell(int r, int c) const {
  int idx = r * cols + c;
  const Cell& cell = cells[idx];
  int x = gridRect.x + c * cellPx;
  int y = gridRect.y + r * cellPx;
  const int pad = 2;
  int w = cellPx - pad;
  int h = cellPx - pad;

  if (!cell.revealed) {
    renderer.fillRoundedRect(x, y, w, h, 3, Color::DarkGray);
    renderer.drawRoundedRect(x, y, w, h, 1, 3, true);
    if (cell.flagged) {
      const char* label = "F";
      int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
      renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, label, true);
    }
    return;
  }

  renderer.fillRoundedRect(x, y, w, h, 3, cell.mine ? Color::Black : Color::White);
  renderer.drawRoundedRect(x, y, w, h, 1, 3, true);
  if (cell.mine) {
    const char* label = "*";
    int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, label, false);
  } else if (cell.adjacent > 0) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", cell.adjacent);
    int tw = renderer.getTextWidth(UI_10_FONT_ID, buf);
    renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, buf, true);
  }
}

void MinesweeperActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MINESWEEPER_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int statusH = 30;
  const int gap = 6;
  const int marginX = 6;

  int minesLeft = mineCount - flagCount;
  if (!firstRevealDone) minesLeft = mineCount;
  char statusBuf[24];
  snprintf(statusBuf, sizeof(statusBuf), "%s: %d", tr(STR_MINESWEEPER_MINES_LEFT), minesLeft);
  renderer.drawText(UI_10_FONT_ID, marginX, contentTop + statusH / 2 - 6, statusBuf, true);

  const int btnW = 90;
  const int btnH = statusH - 4;
  newGameRect = Rect{pageWidth - marginX - btnW, contentTop + 2, btnW, btnH};
  renderer.drawRoundedRect(newGameRect.x, newGameRect.y, newGameRect.width, newGameRect.height, 1, 4, true);
  {
    const char* label = tr(STR_MINESWEEPER_NEW_GAME);
    int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, newGameRect.x + (newGameRect.width - tw) / 2, newGameRect.y + newGameRect.height / 2 - 6,
                      label, true);
  }

  const int gridTop = contentTop + statusH + gap;
  const int gridAvailW = pageWidth - 2 * marginX;
  const int gridAvailH = contentHeight - statusH - gap;
  cellPx = std::max(1, std::min(gridAvailW / cols, gridAvailH / rows));
  const int gridW = cellPx * cols;
  const int gridH = cellPx * rows;
  gridRect = Rect{(pageWidth - gridW) / 2, gridTop + (gridAvailH - gridH) / 2, gridW, gridH};

  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      drawCell(r, c);
    }
  }

  if (state != GameState::Playing) {
    const int boxW = pageWidth - 80;
    const int boxH = 100;
    const int boxX = (pageWidth - boxW) / 2;
    const int boxY = contentTop + (contentHeight - boxH) / 2;
    renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
    renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, boxY + 24,
                              state == GameState::Won ? tr(STR_MINESWEEPER_WON) : tr(STR_MINESWEEPER_LOST), true);
    renderer.drawCenteredText(UI_10_FONT_ID, boxY + 56, tr(STR_MINESWEEPER_TAP_NEW_GAME), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MINESWEEPER_NEW_GAME), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (forceFullRefresh) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    forceFullRefresh = false;
  } else {
    renderer.displayBuffer();
  }
}
