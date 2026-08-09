#include "SudokuActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kStateFilePath = "/.crosspoint/sudoku.bin";
constexpr int kN = 9;
constexpr int kNN = kN * kN;

bool rectContains(const Rect& r, int x, int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

// Bitmask (bit d-1 => digit d present) of digits already used in idx's row/column/box.
uint16_t usedMask(const std::vector<uint8_t>& grid, int idx) {
  int r = idx / kN;
  int c = idx % kN;
  int boxR = (r / 3) * 3;
  int boxC = (c / 3) * 3;
  uint16_t mask = 0;
  for (int i = 0; i < kN; i++) {
    uint8_t v = grid[r * kN + i];
    if (v != 0) mask |= static_cast<uint16_t>(1u << (v - 1));
    v = grid[i * kN + c];
    if (v != 0) mask |= static_cast<uint16_t>(1u << (v - 1));
  }
  for (int dr = 0; dr < 3; dr++) {
    for (int dc = 0; dc < 3; dc++) {
      uint8_t v = grid[(boxR + dr) * kN + (boxC + dc)];
      if (v != 0) mask |= static_cast<uint16_t>(1u << (v - 1));
    }
  }
  return mask;
}

// Iterative (no C++ recursion), MRV-pruned solution counter, capped at maxSolutions and
// maxNodes. Returns the solution count found (0, 1, or maxSolutions once reached), or -1 if
// the node budget was exhausted before the search could conclude (treated as "not provably
// unique" by the caller, i.e. the clue is kept).
int countSolutions(std::vector<uint8_t> grid, int maxSolutions, int maxNodes) {
  struct Frame {
    int cell;
    uint16_t triedMask;
  };
  std::vector<Frame> stack;
  stack.reserve(kNN);
  int solutions = 0;
  int nodes = 0;

  while (true) {
    int cell = -1;
    int bestCount = 10;
    uint16_t candMask = 0;
    bool dead = false;
    for (int i = 0; i < kNN; i++) {
      if (grid[i] != 0) continue;
      uint16_t cand = static_cast<uint16_t>((~usedMask(grid, i)) & 0x1FFu);
      int cnt = __builtin_popcount(cand);
      if (cnt == 0) {
        dead = true;
        break;
      }
      if (cnt < bestCount) {
        bestCount = cnt;
        cell = i;
        candMask = cand;
        if (cnt == 1) break;
      }
    }

    if (!dead && cell == -1) {
      // No empty cells left: a full solution.
      solutions++;
      if (solutions >= maxSolutions) return solutions;
      dead = true;  // fall through to backtrack and look for another solution
    }

    if (dead) {
      bool backtracked = false;
      while (!stack.empty()) {
        Frame& top = stack.back();
        grid[top.cell] = 0;
        uint16_t remaining = static_cast<uint16_t>((~usedMask(grid, top.cell)) & 0x1FFu & ~top.triedMask);
        if (remaining == 0) {
          stack.pop_back();
          continue;
        }
        int digit = __builtin_ctz(remaining) + 1;
        top.triedMask = static_cast<uint16_t>(top.triedMask | (1u << (digit - 1)));
        grid[top.cell] = static_cast<uint8_t>(digit);
        backtracked = true;
        break;
      }
      if (!backtracked) return solutions;
      if (++nodes >= maxNodes) return -1;
      continue;
    }

    int digit = __builtin_ctz(candMask) + 1;
    grid[cell] = static_cast<uint8_t>(digit);
    stack.push_back(Frame{cell, static_cast<uint16_t>(1u << (digit - 1))});
    if (++nodes >= maxNodes) return -1;
  }
}

void transposeGrid(std::vector<uint8_t>& grid) {
  std::vector<uint8_t> t(kNN);
  for (int r = 0; r < kN; r++) {
    for (int c = 0; c < kN; c++) {
      t[c * kN + r] = grid[r * kN + c];
    }
  }
  grid = t;
}

// Row order that keeps each band (3 rows) intact but shuffles rows within a band and
// shuffles band order -- the standard Sudoku-preserving row permutation. Applying this
// twice (once directly, once around a transpose) permutes rows, then columns.
void randomBandRowOrder(int order[kN]) {
  int bands[3] = {0, 1, 2};
  for (int i = 2; i > 0; i--) {
    int j = static_cast<int>(random(i + 1));
    std::swap(bands[i], bands[j]);
  }
  int idx = 0;
  for (int b = 0; b < 3; b++) {
    int rows[3] = {0, 1, 2};
    for (int i = 2; i > 0; i--) {
      int j = static_cast<int>(random(i + 1));
      std::swap(rows[i], rows[j]);
    }
    for (int i = 0; i < 3; i++) order[idx++] = bands[b] * 3 + rows[i];
  }
}

void permuteRowsRandom(std::vector<uint8_t>& grid) {
  int order[kN];
  randomBandRowOrder(order);
  std::vector<uint8_t> t(kNN);
  for (int i = 0; i < kN; i++) {
    memcpy(&t[i * kN], &grid[order[i] * kN], kN);
  }
  grid = t;
}
}  // namespace

void SudokuActivity::onEnter() {
  Activity::onEnter();
  loadOrGenerate();
  requestUpdate();
}

void SudokuActivity::loadOrGenerate() {
  bool loaded = false;
  HalFile file;
  if (Storage.openFileForRead("SUDOKU", kStateFilePath, file)) {
    uint8_t buf[sizeof(uint8_t) + sizeof(State)];
    const int readLen = file.read(buf, sizeof(buf));
    if (readLen == static_cast<int>(sizeof(buf)) && buf[0] == kStateFileVersion) {
      memcpy(&state, buf + 1, sizeof(State));
      syncBoardFromState();
      loaded = true;
    }
  }
  if (!loaded) {
    startNewGame(Difficulty::Easy);
  }
}

void SudokuActivity::save() {
  syncStateFromBoard();
  HalFile file;
  if (Storage.openFileForWrite("SUDOKU", kStateFilePath, file)) {
    uint8_t buf[sizeof(uint8_t) + sizeof(State)];
    buf[0] = kStateFileVersion;
    memcpy(buf + 1, &state, sizeof(State));
    file.write(buf, sizeof(buf));
  }
}

void SudokuActivity::syncStateFromBoard() {
  state.difficulty = static_cast<uint8_t>(difficulty);
  state.completed = completed ? 1 : 0;
  memset(state.givenMask, 0, sizeof(state.givenMask));
  for (int i = 0; i < kCells; i++) {
    if (given[i]) state.givenMask[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
    state.board[i] = board[i];
  }
}

void SudokuActivity::syncBoardFromState() {
  difficulty = static_cast<Difficulty>(state.difficulty);
  completed = state.completed != 0;
  board.assign(kCells, 0);
  given.assign(kCells, 0);
  for (int i = 0; i < kCells; i++) {
    given[i] = static_cast<uint8_t>((state.givenMask[i / 8] >> (i % 8)) & 1u);
    board[i] = state.board[i];
  }
  selectedIdx = -1;
}

void SudokuActivity::startNewGame(Difficulty newDifficulty) {
  difficulty = newDifficulty;

  // Base valid grid (a well-known formula), then randomize via digit relabeling and
  // band/stack-preserving row/column permutation + optional transpose -- O(81), no
  // backtracking needed to produce a full solved grid.
  std::vector<uint8_t> solved(kCells);
  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      solved[r * kSize + c] = static_cast<uint8_t>((r * 3 + r / 3 + c) % 9 + 1);
    }
  }
  uint8_t remap[10];
  for (int d = 1; d <= 9; d++) remap[d] = static_cast<uint8_t>(d);
  for (int i = 9; i > 1; i--) {
    int j = 1 + static_cast<int>(random(i));
    std::swap(remap[i], remap[j]);
  }
  for (auto& v : solved) v = remap[v];

  permuteRowsRandom(solved);
  transposeGrid(solved);
  permuteRowsRandom(solved);  // permutes what were columns
  transposeGrid(solved);
  if (random(2) == 0) transposeGrid(solved);

  int targetClues = 42;
  switch (difficulty) {
    case Difficulty::Easy:
      targetClues = 42;
      break;
    case Difficulty::Medium:
      targetClues = 34;
      break;
    case Difficulty::Hard:
      targetClues = 27;
      break;
  }

  // Self-terminating single pass: shuffle cell order, tentatively clear each, keep it
  // cleared only if the puzzle still has exactly one solution.
  std::vector<int> order(kCells);
  for (int i = 0; i < kCells; i++) order[i] = i;
  for (int i = kCells - 1; i > 0; i--) {
    int j = static_cast<int>(random(i + 1));
    std::swap(order[i], order[j]);
  }

  board = solved;
  given.assign(kCells, 1);
  int clues = kCells;
  for (int idx : order) {
    if (clues <= targetClues) break;
    uint8_t saved = board[idx];
    board[idx] = 0;
    int result = countSolutions(board, 2, kMaxSolverNodes);
    if (result == 1) {
      given[idx] = 0;
      clues--;
    } else {
      board[idx] = saved;
    }
  }

  completed = false;
  selectedIdx = -1;
  forceFullRefresh = true;
  save();
  requestUpdate();
}

bool SudokuActivity::isGiven(int idx) const {
  return idx >= 0 && idx < kCells && given[idx] != 0;
}

// True when board[idx]'s value duplicates another cell's value in the same
// row, column or box. Used purely for highlighting -- placements are never
// blocked, so this can legitimately be true for several cells at once.
bool SudokuActivity::cellConflicts(int idx) const {
  uint8_t digit = board[idx];
  if (digit == 0) return false;
  int r = idx / kSize;
  int c = idx % kSize;
  int boxR = (r / 3) * 3;
  int boxC = (c / 3) * 3;
  for (int i = 0; i < kSize; i++) {
    if (i != c && board[r * kSize + i] == digit) return true;
    if (i != r && board[i * kSize + c] == digit) return true;
  }
  for (int dr = 0; dr < 3; dr++) {
    for (int dc = 0; dc < 3; dc++) {
      int idx2 = (boxR + dr) * kSize + (boxC + dc);
      if (idx2 != idx && board[idx2] == digit) return true;
    }
  }
  return false;
}

void SudokuActivity::applyDigit(int digit) {
  if (completed || selectedIdx < 0 || isGiven(selectedIdx)) return;

  if (digit == 0) {
    if (board[selectedIdx] != 0) {
      board[selectedIdx] = 0;
      save();
    }
    forceFullRefresh = true;
    requestUpdate();
    return;
  }

  // Placed freely, even if it duplicates another cell -- render()/drawCell()
  // highlight every conflicting cell instead of blocking the entry.
  board[selectedIdx] = static_cast<uint8_t>(digit);
  checkCompletion();
  forceFullRefresh = true;
  save();
  requestUpdate();
}

void SudokuActivity::checkCompletion() {
  for (int i = 0; i < kCells; i++) {
    if (board[i] == 0) return;
  }
  for (int i = 0; i < kCells; i++) {
    if (cellConflicts(i)) return;
  }
  completed = true;
}

int SudokuActivity::cellIndexAt(int tx, int ty) const {
  if (!rectContains(gridRect, tx, ty) || cellPx <= 0) return -1;
  int c = (tx - gridRect.x) / cellPx;
  int r = (ty - gridRect.y) / cellPx;
  if (c < 0 || c >= kSize || r < 0 || r >= kSize) return -1;
  return r * kSize + c;
}

void SudokuActivity::showMenu() {
  static const StrId options[] = {StrId::STR_SUDOKU_NEW_GAME, StrId::STR_SUDOKU_ABANDON};
  menuPopup.show(StrId::STR_SUDOKU_MENU, options, 2, 0, [this](int idx) {
    if (idx == 0) {
      showDifficultyPicker();
    } else {
      showAbandonConfirm();
    }
  });
  requestUpdate();
}

void SudokuActivity::showDifficultyPicker() {
  static const StrId options[] = {StrId::STR_SUDOKU_DIFFICULTY_EASY, StrId::STR_SUDOKU_DIFFICULTY_MEDIUM,
                                   StrId::STR_SUDOKU_DIFFICULTY_HARD};
  difficultyPopup.show(StrId::STR_SUDOKU_NEW_GAME, options, 3, static_cast<int>(difficulty), [this](int idx) {
    startNewGame(static_cast<Difficulty>(idx));
  });
  requestUpdate();
}

void SudokuActivity::showAbandonConfirm() {
  static const StrId options[] = {StrId::STR_YES, StrId::STR_NO};
  abandonConfirmPopup.show(StrId::STR_SUDOKU_CONFIRM_ABANDON, options, 2, 1, [this](int idx) {
    if (idx == 0) {
      Storage.remove(kStateFilePath);
      finish();
    }
  });
  requestUpdate();
}

void SudokuActivity::loop() {
  using Button = MappedInputManager::Button;

  if (abandonConfirmPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (difficultyPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (menuPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  int tx = 0;
  int ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return;

  if (rectContains(menuButtonRect, tx, ty)) {
    showMenu();
    return;
  }

  int cell = cellIndexAt(tx, ty);
  if (cell >= 0) {
    if (!isGiven(cell)) {
      selectedIdx = cell;
    }
    requestUpdate();
    return;
  }

  for (int i = 0; i < kSize + 1; i++) {
    if (rectContains(digitRects[i], tx, ty)) {
      applyDigit(i == kSize ? 0 : i + 1);
      return;
    }
  }
}

void SudokuActivity::drawCell(int r, int c) const {
  int idx = r * kSize + c;
  int x = gridRect.x + c * cellPx;
  int y = gridRect.y + r * cellPx;
  const int pad = 2;
  int w = cellPx - pad;
  int h = cellPx - pad;

  const bool isSelected = idx == selectedIdx;
  const bool isConflict = cellConflicts(idx);

  Color fill = Color::White;
  if (isGiven(idx)) fill = Color::LightGray;
  renderer.fillRoundedRect(x, y, w, h, 2, fill);
  renderer.drawRoundedRect(x, y, w, h, isConflict ? 3 : (isSelected ? 2 : 1), 2, true);

  uint8_t v = board[idx];
  if (v != 0) {
    char buf[2] = {static_cast<char>('0' + v), '\0'};
    int tw = renderer.getTextWidth(UI_10_FONT_ID, buf);
    renderer.drawText(UI_10_FONT_ID, x + (w - tw) / 2, y + h / 2 - 6, buf, true);
  }
}

void SudokuActivity::render(RenderLock&&) {
  if (abandonConfirmPopup.processRender(renderer, mappedInput)) return;
  if (difficultyPopup.processRender(renderer, mappedInput)) return;
  if (menuPopup.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_SUDOKU_TITLE));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int statusH = 30;
  const int gap = 6;
  const int marginX = 6;
  // Two-row digit pad (5 cols x 2 rows for 9 digits + erase) per Stuart's
  // explicit ask for bigger, easier tap targets -- previously a single row
  // of 10 cramped cells.
  constexpr int kPadCols = 5;
  const int digitPadRowH = 44;
  const int digitPadRowGap = 4;
  const int digitStripH = digitPadRowH * 2 + digitPadRowGap;

  const int btnW = 90;
  const int btnH = statusH - 4;
  menuButtonRect = Rect{pageWidth - marginX - btnW, contentTop + 2, btnW, btnH};
  renderer.drawRoundedRect(menuButtonRect.x, menuButtonRect.y, menuButtonRect.width, menuButtonRect.height, 1, 4, true);
  {
    const char* label = tr(STR_SUDOKU_MENU);
    int tw = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, menuButtonRect.x + (menuButtonRect.width - tw) / 2,
                       menuButtonRect.y + menuButtonRect.height / 2 - 6, label, true);
  }

  const int gridTop = contentTop + statusH + gap;
  const int gridAvailW = pageWidth - 2 * marginX;
  const int gridAvailH = contentHeight - statusH - gap - digitStripH - gap;
  cellPx = std::max(1, std::min(gridAvailW / kSize, gridAvailH / kSize));
  const int gridW = cellPx * kSize;
  const int gridH = cellPx * kSize;
  gridRect = Rect{(pageWidth - gridW) / 2, gridTop, gridW, gridH};

  for (int r = 0; r < kSize; r++) {
    for (int c = 0; c < kSize; c++) {
      drawCell(r, c);
    }
  }

  // Thicker bars over the 3x3 box boundaries -- drawCell()'s per-cell borders
  // are all the same 1px weight, so the box structure wasn't visually
  // distinguishable from the regular cell gridlines.
  constexpr int kBoxLineW = 3;
  for (int i = 0; i <= kSize; i += 3) {
    renderer.fillRect(gridRect.x + i * cellPx - kBoxLineW / 2, gridRect.y, kBoxLineW, gridRect.height, true);
    renderer.fillRect(gridRect.x, gridRect.y + i * cellPx - kBoxLineW / 2, gridRect.width, kBoxLineW, true);
  }

  const int stripTop = gridRect.y + gridRect.height + gap;
  const int stripCellW = std::max(1, gridAvailW / kPadCols);
  const int stripX = (pageWidth - stripCellW * kPadCols) / 2;
  for (int i = 0; i < kSize + 1; i++) {
    const int row = i / kPadCols;
    const int col = i % kPadCols;
    Rect r = Rect(stripX + col * stripCellW, stripTop + row * (digitPadRowH + digitPadRowGap), stripCellW - 4,
                  digitPadRowH);
    digitRects[i] = r;
    renderer.drawRoundedRect(r.x, r.y, r.width, r.height, 1, 4, true);
    char buf[2] = {i == kSize ? 'X' : static_cast<char>('1' + i), '\0'};
    int tw = renderer.getTextWidth(UI_12_FONT_ID, buf);
    renderer.drawText(UI_12_FONT_ID, r.x + (r.width - tw) / 2, r.y + r.height / 2 - 8, buf, true);
  }

  if (completed) {
    const int boxW = pageWidth - 80;
    const int boxH = 100;
    const int boxX = (pageWidth - boxW) / 2;
    const int boxY = contentTop + (contentHeight - boxH) / 2;
    renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
    renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, boxY + 24, tr(STR_SUDOKU_WON), true);
    renderer.drawCenteredText(UI_10_FONT_ID, boxY + 56, tr(STR_SUDOKU_TAP_NEW_GAME), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SUDOKU_MENU), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (forceFullRefresh) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    forceFullRefresh = false;
  } else {
    renderer.displayBuffer();
  }
}
