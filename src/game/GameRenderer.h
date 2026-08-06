#pragma once

#include <GfxRenderer.h>

#include "GameTypes.h"
#include "MappedInputManager.h"

class GameState;

// Renders the dungeon viewport, status bar, message log, and button hints.
// The hints bar also doubles as the on-screen touch control strip: each of its
// 6 columns is both a text label and a tap target (see hitTestHints()).
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
  // 56px gives 6 equal ~80px-wide touch columns at 480px screen width — comfortably
  // above the 44x44 minimum recommended touch target, while only costing one fewer
  // dungeon row than the old 34px text-only hint bar.
  static constexpr int HINTS_H = 56;
  static constexpr int HINT_BUTTON_COUNT = 6;

  // Computed at init
  int viewportW = 0;   // Pixels
  int viewportH = 0;   // Pixels
  int viewCols = 0;    // Grid columns
  int viewRows = 0;    // Grid rows
  int viewportEndY = 0;
  int messageY = 0;
  int hintsY = 0;
  int screenW = 0;
  int screenH = 0;
  int gridOffsetX = 0; // Left padding to center grid

  void init(GfxRenderer& renderer);

  // Draw the full game screen
  void draw(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar, const game::Monster* monsters,
            uint8_t monsterCount, const game::Item* items, uint8_t itemCount, const bool* visible);

  // Hit-tests a tap point against the hints bar's 6 touch columns (Back, Confirm,
  // Left, Right, Up, Down — same order as drawHints() draws them). Returns true and
  // sets outButton if the tap landed on a column, false otherwise.
  bool hitTestHints(int x, int y, MappedInputManager::Button& outButton) const;

 private:
  void drawStatusBar(GfxRenderer& renderer) const;
  void drawViewport(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar,
                    const game::Monster* monsters, uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                    const bool* visible) const;
  void drawCell(GfxRenderer& renderer, int screenX, int screenY, char glyph, bool isVisible, bool isExplored) const;
  void drawMessages(GfxRenderer& renderer) const;
  void drawHints(GfxRenderer& renderer) const;
};
