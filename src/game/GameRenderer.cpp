#include "GameRenderer.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "GameSprites.h"
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
  // Refresh once per render pass (not per-cell) so a menu-driven theme change
  // takes effect on the next draw without any extra wiring.
  activeTheme = game::getTheme(static_cast<game::GameThemeId>(SETTINGS.gameTheme));

  renderer.clearScreen();

  drawStatusBar(renderer);
  drawViewport(renderer, tiles, fogOfWar, monsters, monsterCount, items, itemCount, visible);
  drawMessages(renderer);
  drawControls(renderer);

  // Separator lines
  renderer.drawLine(0, STATUS_H, screenW, STATUS_H);
  renderer.drawLine(0, viewportEndY, screenW, viewportEndY);
  renderer.drawLine(0, controlsY, screenW, controlsY);

  renderer.displayBufferGhostGuard(ghostGuardCounter, SETTINGS.getRefreshFrequency(), HalDisplay::FAST_REFRESH);
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

      // Determine what glyph to show, and the theme-resolved sprite (if any)
      // for the same occupant, mirroring the glyph priority order exactly
      // (player > monster > item > tile).
      char glyph = game::tileGlyph(tiles[mapIdx]);
      const Sprite2bpp* sprite = activeTheme->tiles[static_cast<size_t>(tiles[mapIdx])];

      // If currently visible, check for monsters and items on this tile
      if (isVisible) {
        // Player
        if (mapX == p.x && mapY == p.y) {
          glyph = '@';
          sprite = activeTheme->player;
        } else {
          // Check monsters
          bool foundMonster = false;
          for (uint8_t m = 0; m < monsterCount; m++) {
            if (monsters[m].x == mapX && monsters[m].y == mapY && monsters[m].hp > 0) {
              glyph = game::MONSTER_DEFS[monsters[m].type].glyph;
              sprite = activeTheme->monsters[monsters[m].type];
              foundMonster = true;
              break;
            }
          }
          // Check items (only if no monster shown)
          if (!foundMonster) {
            for (uint8_t i = 0; i < itemCount; i++) {
              if (items[i].x == mapX && items[i].y == mapY) {
                glyph = game::itemGlyph(items[i].type);
                sprite = nullptr;
                for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
                  if (game::ITEM_DEFS[d].type == items[i].type && game::ITEM_DEFS[d].subtype == items[i].subtype) {
                    sprite = activeTheme->items[d];
                    break;
                  }
                }
                break;
              }
            }
          }
        }
      }

      drawCell(renderer, screenCellX, screenCellY, glyph, sprite, isVisible, isExplored);
    }
  }
}

void GameRenderer::drawCell(GfxRenderer& renderer, int screenX, int screenY, char glyph, const Sprite2bpp* sprite,
                            bool isVisible, bool isExplored) const {
  // "No art" means either a null pointer, or a Sprite2bpp with a null data
  // pointer -- both fall through to the original glyph rendering below,
  // unchanged from the pre-theme implementation.
  bool haveSprite = sprite != nullptr && sprite->data != nullptr;

  if (isVisible) {
    if (haveSprite) {
      // Center the sprite horizontally in the cell, matching the glyph's
      // vertical anchor (screenY - 2) as closely as a differently-sized
      // sprite allows.
      int offsetX = (CELL_W - static_cast<int>(sprite->w)) / 2;
      drawSprite(renderer, screenX + offsetX, screenY - 2, *sprite);
    } else {
      // Visible: black character on white background
      char buf[2] = {glyph, '\0'};
      // Center character horizontally in cell
      int charW = renderer.getTextWidth(UI_10_FONT_ID, buf);
      int offsetX = (CELL_W - charW) / 2;
      renderer.drawText(UI_10_FONT_ID, screenX + offsetX, screenY - 2, buf, true);
    }
  } else if (isExplored) {
    // Remembered: gray dithered background regardless of sprite/glyph mode.
    renderer.fillRectDither(screenX, screenY, CELL_W, CELL_H, LightGray);
    if (haveSprite) {
      int offsetX = (CELL_W - static_cast<int>(sprite->w)) / 2;
      drawSprite(renderer, screenX + offsetX, screenY - 2, *sprite);
    } else {
      char buf[2] = {glyph, '\0'};
      int charW = renderer.getTextWidth(UI_10_FONT_ID, buf);
      int offsetX = (CELL_W - charW) / 2;
      renderer.drawText(UI_10_FONT_ID, screenX + offsetX, screenY - 2, buf, true);
    }
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

void GameRenderer::drawEndScreen(GfxRenderer& renderer, bool isVictory, const EndScreenData& data) const {
  // Deliberately no clearScreen() -- see header comment. The box below fully
  // occludes its own footprint; whatever's still visible outside its edges
  // (status bar/viewport/controls from the frame this overlay landed on) is
  // acceptable leftover for a one-shot modal.
  const int boxW = screenW - 60;
  const int boxH = 360;
  const int boxX = (screenW - boxW) / 2;
  const int boxY = (screenH - boxH) / 2;

  renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
  renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);

  int y = boxY + 30;
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y, isVictory ? tr(STR_DM_VICTORY) : tr(STR_DM_YOU_DIED), true);
  y += 34;

  char line[64];
  if (!isVictory) {
    snprintf(line, sizeof(line), "%s %s", tr(STR_DM_DEATH_CAUSE), data.cause);
    renderer.drawCenteredText(UI_12_FONT_ID, y, line, true);
    y += 26;
  }

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_FLOOR), data.floor);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_TURNS), data.turns);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_KILLS), data.kills);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_LEVEL), data.level);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true);
  y += 30;

  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_DM_ACHIEVEMENTS), true);
  y += 22;

  if (data.unlockedCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_DM_NO_ACHIEVEMENTS), true);
  } else {
    for (uint8_t i = 0; i < data.unlockedCount; i++) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, game::achievementShortName(data.unlockedIds[i]), true);
      y += 18;
    }
  }

  renderer.drawCenteredText(UI_10_FONT_ID, boxY + boxH - 24, tr(STR_DM_TAP_TO_CONTINUE), true);

  // One-shot full-refresh: new high-contrast content over a stale buffer
  // needs a clean waveform, not the periodic ghost-guard cadence draw() uses.
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
