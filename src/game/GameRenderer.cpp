#include "GameRenderer.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "GameState.h"
#include "fontIds.h"

void GameRenderer::init(GfxRenderer& renderer) {
  screenW = renderer.getScreenWidth();
  screenH = renderer.getScreenHeight();

  // Compute viewport dimensions
  viewportEndY = screenH - MESSAGE_H - CONTROLS_H;
  viewportH = viewportEndY - VIEWPORT_Y;
  viewportW = screenW;

  viewCols = viewportW / CELL_W;
  viewRows = viewportH / CELL_H;

  // Center the grid horizontally if there's leftover space
  gridOffsetX = (viewportW - viewCols * CELL_W) / 2;

  messageY = viewportEndY;
  controlsY = screenH - CONTROLS_H;
}

void GameRenderer::draw(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar,
                        const game::Monster* monsters, uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                        const bool* visible) {
  renderer.clearScreen();

  drawStatusBar(renderer);
  drawViewport(renderer, tiles, fogOfWar, monsters, monsterCount, items, itemCount, visible);
  drawMessages(renderer);
  drawControls(renderer);

  // Separator lines
  renderer.drawLine(0, STATUS_H, screenW, STATUS_H);
  renderer.drawLine(0, viewportEndY, screenW, viewportEndY);
  renderer.drawLine(0, controlsY, screenW, controlsY);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

// --- Status Bar ---

void GameRenderer::drawStatusBar(GfxRenderer& renderer) const {
  const auto& p = GAME_STATE.player;

  char hpBuf[24];
  snprintf(hpBuf, sizeof(hpBuf), "HP:%u/%u", p.hp, p.maxHp);

  char mpBuf[24];
  snprintf(mpBuf, sizeof(mpBuf), "MP:%u/%u", p.mp, p.maxMp);

  char depthBuf[16];
  snprintf(depthBuf, sizeof(depthBuf), "Dl:%u", p.dungeonDepth);

  char lvlBuf[16];
  snprintf(lvlBuf, sizeof(lvlBuf), "Cl:%u", p.charLevel);

  // Left side: HP and MP
  renderer.drawText(UI_10_FONT_ID, 4, STATUS_Y, hpBuf, true, EpdFontFamily::BOLD);
  int hpWidth = renderer.getTextWidth(UI_10_FONT_ID, hpBuf, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, 4 + hpWidth + 10, STATUS_Y, mpBuf);

  // Right side: Dungeon level and Character level
  int lvlWidth = renderer.getTextWidth(UI_10_FONT_ID, lvlBuf);
  int depthWidth = renderer.getTextWidth(UI_10_FONT_ID, depthBuf);
  renderer.drawText(UI_10_FONT_ID, screenW - lvlWidth - 4, STATUS_Y, lvlBuf);
  renderer.drawText(UI_10_FONT_ID, screenW - lvlWidth - depthWidth - 14, STATUS_Y, depthBuf);
}

// --- Viewport ---

void GameRenderer::drawViewport(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar,
                                const game::Monster* monsters, uint8_t monsterCount, const game::Item* items,
                                uint8_t itemCount, const bool* visible) const {
  const auto& p = GAME_STATE.player;

  // Calculate viewport origin (center on player, clamped to map bounds)
  int viewX = p.x - viewCols / 2;
  int viewY = p.y - viewRows / 2;
  viewX = std::max(0, std::min(viewX, game::MAP_WIDTH - viewCols));
  viewY = std::max(0, std::min(viewY, game::MAP_HEIGHT - viewRows));

  // Draw each cell in the viewport
  for (int row = 0; row < viewRows; row++) {
    int mapY = viewY + row;
    if (mapY < 0 || mapY >= game::MAP_HEIGHT) continue;

    int screenCellY = VIEWPORT_Y + row * CELL_H;

    for (int col = 0; col < viewCols; col++) {
      int mapX = viewX + col;
      if (mapX < 0 || mapX >= game::MAP_WIDTH) continue;

      int mapIdx = mapY * game::MAP_WIDTH + mapX;
      int screenCellX = gridOffsetX + col * CELL_W;

      bool isExplored = game::fogIsExplored(fogOfWar, mapX, mapY);
      bool isVisible = visible[mapIdx];

      if (!isExplored && !isVisible) {
        // Unseen tile — leave white (cleared screen)
        continue;
      }

      // Determine what glyph to show
      char glyph = game::tileGlyph(tiles[mapIdx]);

      // If currently visible, check for monsters and items on this tile
      if (isVisible) {
        // Player
        if (mapX == p.x && mapY == p.y) {
          glyph = '@';
        } else {
          // Check monsters
          for (uint8_t m = 0; m < monsterCount; m++) {
            if (monsters[m].x == mapX && monsters[m].y == mapY && monsters[m].hp > 0) {
              glyph = game::MONSTER_DEFS[monsters[m].type].glyph;
              break;
            }
          }
          // Check items (only if no monster shown)
          if (glyph == game::tileGlyph(tiles[mapIdx])) {
            for (uint8_t i = 0; i < itemCount; i++) {
              if (items[i].x == mapX && items[i].y == mapY) {
                glyph = game::itemGlyph(items[i].type);
                break;
              }
            }
          }
        }
      }

      drawCell(renderer, screenCellX, screenCellY, glyph, isVisible, isExplored);
    }
  }
}

void GameRenderer::drawCell(GfxRenderer& renderer, int screenX, int screenY, char glyph, bool isVisible,
                            bool isExplored) const {
  if (isVisible) {
    // Visible: black character on white background
    char buf[2] = {glyph, '\0'};
    // Center character horizontally in cell
    int charW = renderer.getTextWidth(UI_10_FONT_ID, buf);
    int offsetX = (CELL_W - charW) / 2;
    renderer.drawText(UI_10_FONT_ID, screenX + offsetX, screenY - 2, buf, true);
  } else if (isExplored) {
    // Remembered: gray dithered character
    renderer.fillRectDither(screenX, screenY, CELL_W, CELL_H, LightGray);
    char buf[2] = {glyph, '\0'};
    int charW = renderer.getTextWidth(UI_10_FONT_ID, buf);
    int offsetX = (CELL_W - charW) / 2;
    renderer.drawText(UI_10_FONT_ID, screenX + offsetX, screenY - 2, buf, true);
  }
}

// --- Message Log ---

void GameRenderer::drawMessages(GfxRenderer& renderer) const {
  // Show 2 most recent messages
  const auto& msg0 = GAME_STATE.getMessage(0);
  const auto& msg1 = GAME_STATE.getMessage(1);

  if (!msg1.empty()) {
    renderer.drawText(SMALL_FONT_ID, 4, messageY + 1, msg1.c_str());
  }
  if (!msg0.empty()) {
    renderer.drawText(SMALL_FONT_ID, 4, messageY + 19, msg0.c_str());
  }
}

// --- On-Screen Controls (D-Pad + Action/Menu) ---

namespace {
// Draws a simple filled triangle arrow glyph centered in [cx, cy], pointing in the
// given direction. Uses GfxRenderer::fillPolygon (no reusable arrow icon glyph exists
// elsewhere in the codebase — UIIcon only covers file-browser/nav iconography).
enum class ArrowDir { Up, Down, Left, Right };

void drawArrow(GfxRenderer& renderer, int cx, int cy, ArrowDir dir) {
  constexpr int kArrowSize = 10;  // Half-extent of the arrow glyph, in pixels
  int xs[3];
  int ys[3];
  switch (dir) {
    case ArrowDir::Up:
      xs[0] = cx - kArrowSize; ys[0] = cy + kArrowSize;
      xs[1] = cx + kArrowSize; ys[1] = cy + kArrowSize;
      xs[2] = cx;              ys[2] = cy - kArrowSize;
      break;
    case ArrowDir::Down:
      xs[0] = cx - kArrowSize; ys[0] = cy - kArrowSize;
      xs[1] = cx + kArrowSize; ys[1] = cy - kArrowSize;
      xs[2] = cx;              ys[2] = cy + kArrowSize;
      break;
    case ArrowDir::Left:
      xs[0] = cx + kArrowSize; ys[0] = cy - kArrowSize;
      xs[1] = cx + kArrowSize; ys[1] = cy + kArrowSize;
      xs[2] = cx - kArrowSize; ys[2] = cy;
      break;
    case ArrowDir::Right:
      xs[0] = cx - kArrowSize; ys[0] = cy - kArrowSize;
      xs[1] = cx - kArrowSize; ys[1] = cy + kArrowSize;
      xs[2] = cx + kArrowSize; ys[2] = cy;
      break;
  }
  renderer.fillPolygon(xs, ys, 3, true);
}
}  // namespace

void GameRenderer::drawControls(GfxRenderer& renderer) const {
  // Left side: 3x3 d-pad cross (Up/Left/Right/Down), no visible borders on the
  // individual cells — the cross shape itself is the affordance, matching common
  // on-screen d-pad conventions. Center cell is decorative/unused (no button there).
  const int dpadRow0Y = controlsY;
  const int dpadRow1Y = controlsY + CONTROL_ROW_H;
  const int dpadRow2Y = controlsY + CONTROL_ROW_H * 2;

  const int dpadColMidX = DPAD_COL_W + DPAD_COL_W / 2;       // center of middle column
  const int dpadColLeftX = DPAD_COL_W / 2;                   // center of left column
  const int dpadColRightX = DPAD_COL_W * 2 + DPAD_COL_W / 2; // center of right column

  drawArrow(renderer, dpadColMidX, dpadRow0Y + CONTROL_ROW_H / 2, ArrowDir::Up);
  drawArrow(renderer, dpadColLeftX, dpadRow1Y + CONTROL_ROW_H / 2, ArrowDir::Left);
  drawArrow(renderer, dpadColRightX, dpadRow1Y + CONTROL_ROW_H / 2, ArrowDir::Right);
  drawArrow(renderer, dpadColMidX, dpadRow2Y + CONTROL_ROW_H / 2, ArrowDir::Down);

  // Right side: Action (top) / Menu (bottom) bordered buttons, stacked, spanning the
  // remaining width. Each is CONTROL_ROW_H tall with a visible border rectangle.
  const int buttonX = DPAD_W;
  const int buttonW = screenW - DPAD_W;
  const int buttonH = (CONTROLS_H) / ACTION_MENU_BUTTON_COUNT;

  static constexpr StrId kButtonLabelIds[ACTION_MENU_BUTTON_COUNT] = {
      StrId::STR_DM_HINT_ACTION,
      StrId::STR_DM_HINT_MENU,
  };

  for (int i = 0; i < ACTION_MENU_BUTTON_COUNT; i++) {
    const int buttonY = controlsY + i * buttonH;
    renderer.drawRect(buttonX, buttonY, buttonW, buttonH);

    const char* label = I18n::getInstance().get(kButtonLabelIds[i]);
    const int textW = renderer.getTextWidth(SMALL_FONT_ID, label);
    const int textH = renderer.getLineHeight(SMALL_FONT_ID);
    const int textX = buttonX + (buttonW - textW) / 2;
    const int textY = buttonY + (buttonH - textH) / 2;
    renderer.drawText(SMALL_FONT_ID, textX, textY, label);
  }
}

bool GameRenderer::hitTestControls(int x, int y, MappedInputManager::Button& outButton) const {
  if (y < controlsY || y >= screenH || x < 0 || x >= screenW) {
    return false;
  }

  if (x < DPAD_W) {
    // D-pad region: 3x3 grid. Row 0 => Up, Row 1 => Left/[center: no-op]/Right, Row 2 => Down.
    int row = (y - controlsY) / CONTROL_ROW_H;
    if (row > 2) row = 2;  // absorb any rounding remainder pixels
    int col = x / DPAD_COL_W;
    if (col > 2) col = 2;

    if (row == 0 && col == 1) {
      outButton = MappedInputManager::Button::Up;
      return true;
    }
    if (row == 2 && col == 1) {
      outButton = MappedInputManager::Button::Down;
      return true;
    }
    if (row == 1 && col == 0) {
      outButton = MappedInputManager::Button::Left;
      return true;
    }
    if (row == 1 && col == 2) {
      outButton = MappedInputManager::Button::Right;
      return true;
    }
    // Decorative center cell (row 1, col 1) and the four corner cells are not
    // mapped to any button — fall through to "no hit".
    return false;
  }

  // Action/Menu region: two stacked bordered buttons.
  const int buttonH = CONTROLS_H / ACTION_MENU_BUTTON_COUNT;
  int buttonIndex = (y - controlsY) / buttonH;
  if (buttonIndex >= ACTION_MENU_BUTTON_COUNT) {
    buttonIndex = ACTION_MENU_BUTTON_COUNT - 1;  // absorb any rounding remainder pixels
  }

  outButton = (buttonIndex == 0) ? MappedInputManager::Button::Confirm : MappedInputManager::Button::Back;
  return true;
}
