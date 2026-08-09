#pragma once

#include <cstdint>
#include <vector>

#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "components/themes/BaseTheme.h"

class MappedInputManager;

// Standard 9x9 Sudoku. Puzzles are generated on-device (band/stack/digit
// permutation of a base valid grid, then holes dug one at a time with an
// iterative uniqueness-check solver, MRV-pruned, no recursion, per the
// stack-budget rule) -- never a canned puzzle bank. Three difficulty levels
// select the target clue count. Progress is saved after every single move
// and auto-restored on re-entry (stricter than the debounced-save idiom
// used elsewhere, per Stuart's explicit spec). Placements are never
// rejected -- a digit that duplicates another in the same row, column or
// box is stored anyway, and every cell sharing that conflict (not just the
// one just tapped) is highlighted with a heavier border on every render, so
// the board can transiently be invalid. Win detection therefore requires
// both "no empty cells" and "no conflicted cells". Entry is tap-cell then
// tap-digit (1-9 + erase); no pencil marks (explicitly deferred, v2). The
// in-activity Menu button offers New Game (with a difficulty picker) and
// Abandon (discards the saved game and returns to the games list) via the
// existing OptionPopup component. No engine/shared abstraction with the
// other games -- deliberately kept flat (YAGNI), matching Minesweeper.
class SudokuActivity final : public Activity {
 public:
  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class Difficulty : uint8_t { Easy = 0, Medium = 1, Hard = 2 };

  static constexpr int kSize = 9;
  static constexpr int kCells = kSize * kSize;
  static constexpr uint8_t kStateFileVersion = 1;
  static constexpr int kMaxSolverNodes = 20000;

  // #pragma pack not needed: every field is uint8_t, so the compiler cannot insert padding.
  struct State {
    uint8_t difficulty = 0;
    uint8_t completed = 0;
    uint8_t givenMask[11] = {};  // 81 bits packed, bit=1 => given (non-editable)
    uint8_t board[kCells] = {};  // current values, 0 = empty
  };

  State state;
  std::vector<uint8_t> board;  // mirrors state.board as a working vector, size kCells
  std::vector<uint8_t> given;  // 1 = given/non-editable, 0 = player-editable, size kCells
  Difficulty difficulty = Difficulty::Easy;
  bool completed = false;

  int selectedIdx = -1;

  // Set on every state-changing interaction (new game, digit entry/erase)
  // and consumed once by render(), which then asks for a HALF_REFRESH
  // instead of the default FAST_REFRESH -- mirrors MinesweeperActivity's
  // and SolitaireActivity's existing fix for the same e-ink ghosting
  // complaint (this activity previously had no such pattern at all).
  bool forceFullRefresh = false;

  OptionPopup menuPopup;
  OptionPopup difficultyPopup;
  OptionPopup abandonConfirmPopup;

  // Hit-rects/geometry, recomputed each render() and read back by loop()'s touch handling.
  Rect gridRect;
  Rect menuButtonRect;
  Rect digitRects[kSize + 1];  // 1-9 then erase, laid out two rows deep
  int cellPx = 0;

  void loadOrGenerate();
  void save();
  void startNewGame(Difficulty newDifficulty);
  void syncStateFromBoard();
  void syncBoardFromState();

  bool isGiven(int idx) const;
  bool cellConflicts(int idx) const;
  void applyDigit(int digit);
  void checkCompletion();

  int cellIndexAt(int tx, int ty) const;
  void showMenu();
  void showDifficultyPicker();
  void showAbandonConfirm();

  void drawCell(int r, int c) const;
};
