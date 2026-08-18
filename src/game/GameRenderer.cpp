#include "GameRenderer.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "CrossPointSettings.h"
#include "GameSprites.h"
#include "GameState.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GameRenderer::computeLayout(int screenWidth, int screenHeight, int messageLineHeightIn) {
  screenW = screenWidth;
  screenH = screenHeight;
  messageLineHeight = messageLineHeightIn;
  messageH = MESSAGE_LINE_COUNT * messageLineHeight + MESSAGE_PADDING_V;

  // Compute viewport dimensions. Banner sits between the map and the
  // console -- always reserved, not a floating overlay -- so it comes out of
  // the map's share of the screen, same as the console and controls do.
  viewportEndY = screenH - BANNER_H - messageH - CONTROLS_H;
  viewportH = viewportEndY - VIEWPORT_Y;
  viewportW = screenW;

  viewCols = viewportW / CELL_W;
  viewRows = viewportH / CELL_H;

  // Center the grid horizontally if there's leftover space
  gridOffsetX = (viewportW - viewCols * CELL_W) / 2;

  bannerY = viewportEndY;
  messageY = bannerY + BANNER_H;
  controlsY = screenH - CONTROLS_H;
}

void GameRenderer::init(GfxRenderer& renderer) {
  computeLayout(renderer.getScreenWidth(), renderer.getScreenHeight(), renderer.getLineHeight(UI_10_FONT_ID));
}

void GameRenderer::initForTest(int screenWidth, int screenHeight) {
  // No live GfxRenderer/font table in a host harness -- stand in with
  // UI_10_FONT_ID's real advanceY (24px, confirmed via the on-device build;
  // see drawMessages()/init()) rather than inventing a guessed number.
  constexpr int kTestMessageLineHeight = 24;
  computeLayout(screenWidth, screenHeight, kTestMessageLineHeight);
}

void GameRenderer::draw(GfxRenderer& renderer, const game::Tile* tiles, const uint8_t* fogOfWar,
                        const game::Monster* monsters, uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                        const bool* visible) {
  // Refresh once per render pass (not per-cell) so a menu-driven theme change
  // takes effect on the next draw without any extra wiring.
  activeTheme = game::getTheme(static_cast<game::GameThemeId>(SETTINGS.gameTheme));

  FramePlan plan = planFrame(tiles, fogOfWar, monsters, monsterCount, items, itemCount, visible);

  if (plan.fullClear) {
    renderer.clearScreen();

    drawStatusBar(renderer);
    drawViewport(renderer, tiles, fogOfWar, monsters, monsterCount, items, itemCount, visible);
    drawMessages(renderer);
    drawControls(renderer);
    if (notificationActive_) {
      drawNotification(renderer);
    }

    // Separator lines
    renderer.drawLine(0, STATUS_H, screenW, STATUS_H);
    renderer.drawLine(0, viewportEndY, screenW, viewportEndY);
    renderer.drawLine(0, controlsY, screenW, controlsY);

    renderer.displayBufferGhostGuard(ghostGuardCounter, SETTINGS.getRefreshFrequency(), HalDisplay::FAST_REFRESH);
    return;
  }

  // Partial path: only the regions planFrame() found dirty get erased,
  // redrawn, and refreshed -- everything else already on glass is left
  // untouched. Controls never change frame-to-frame (no dynamic content) so
  // they're never part of a dirty window and never redrawn here.
  const auto& p = GAME_STATE.player;
  int viewX = 0;
  int viewY = 0;
  computeViewOrigin(p.x, p.y, &viewX, &viewY);

  // Fix 1b (parent-agreed, batched with Fix 1/1c): erase+redraw every dirty
  // window into the backbuffer first, but issue exactly ONE displayWindow()
  // refresh per frame -- a union of all the windows' rects -- instead of one
  // refresh per window. Up to 4 separate e-ink refreshes per frame was the
  // other half of the movement-lag report even on frames that stayed on the
  // partial (non-fullClear) path.
  unsigned long partialStartMs = millis();
  int unionX = 0, unionY = 0, unionW = 0, unionH = 0;
  bool haveUnion = false;

  for (int i = 0; i < plan.windowCount; i++) {
    const DirtyWindow& w = plan.windows[i];
    renderer.fillRect(w.x, w.y, w.w, w.h, false);  // erase to white before redraw

    switch (w.kind) {
      case DirtyWindow::Kind::StatusBar:
        drawStatusBar(renderer);
        break;
      case DirtyWindow::Kind::Messages:
        drawMessages(renderer);
        break;
      case DirtyWindow::Kind::Viewport: {
        int colStart = (w.x - gridOffsetX) / CELL_W;
        int rowStart = (w.y - VIEWPORT_Y) / CELL_H;
        int colCount = w.w / CELL_W;
        int rowCount = w.h / CELL_H;
        for (int row = rowStart; row < rowStart + rowCount; row++) {
          for (int col = colStart; col < colStart + colCount; col++) {
            drawViewportCell(renderer, viewX, viewY, row, col, tiles, fogOfWar, monsters, monsterCount, items,
                             itemCount, visible);
          }
        }
        break;
      }
      case DirtyWindow::Kind::Notification:
        // A dismiss only needs the erase-to-white already done above; a live
        // notification also needs its box/title/body painted back in.
        if (notificationActive_) {
          drawNotification(renderer);
        }
        break;
    }

    if (!haveUnion) {
      unionX = w.x;
      unionY = w.y;
      unionW = w.w;
      unionH = w.h;
      haveUnion = true;
    } else {
      int right = unionX + unionW > w.x + w.w ? unionX + unionW : w.x + w.w;
      int bottom = unionY + unionH > w.y + w.h ? unionY + unionH : w.y + w.h;
      unionX = unionX < w.x ? unionX : w.x;
      unionY = unionY < w.y ? unionY : w.y;
      unionW = right - unionX;
      unionH = bottom - unionY;
    }
  }

  if (haveUnion) {
    renderer.displayWindow(unionX, unionY, unionW, unionH);
    LOG_DBG("GAME", "Time = %lu ms for partial refresh, window = %dx%d", millis() - partialStartMs, unionW, unionH);
  }
}

FramePlan GameRenderer::planFrame(const game::Tile* tiles, const uint8_t* fogOfWar, const game::Monster* monsters,
                                  uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                                  const bool* visible) {
  const auto& p = GAME_STATE.player;

  char hpBuf[24];
  char mpBuf[24];
  char depthBuf[16];
  char lvlBuf[16];
  char hungerBuf[16];
  formatStatusBarText(hpBuf, mpBuf, depthBuf, lvlBuf, hungerBuf);

  char msg0[160];
  char msg1[160];
  snprintf(msg0, sizeof(msg0), "%s", GAME_STATE.getMessage(0).c_str());
  snprintf(msg1, sizeof(msg1), "%s", GAME_STATE.getMessage(1).c_str());

  FramePlan plan = planner_.planFrame(buildPlannerLayout(), p.x, p.y, hpBuf, mpBuf, depthBuf, lvlBuf, hungerBuf,
                                      msg0, msg1, tiles, fogOfWar, monsters, monsterCount, items, itemCount, visible,
                                      activeTheme);

  // Notification box is entirely outside FrameDirtyPlanner's own diff logic
  // (it has no concept of it) -- appended here as its own window only when
  // showNotification()/dismissNotification() actually changed state since
  // the last planFrame() call, and only on an already-partial frame (a
  // fullClear frame already repaints everything, notification included, via
  // GameRenderer::draw()'s fullClear branch).
  if (notificationDirty_) {
    notificationDirty_ = false;
    // No ceiling check here: FramePlan::windows is sized for exactly the
    // windows the two owners (FrameDirtyPlanner::planFrame + this
    // notification append) can produce between them -- viewport, status bar,
    // messages, plus this one. A silent drop here was the same "draws
    // outside what it declared" bug class as Q1/Q2/Q3, just inverted: it
    // wouldn't draw outside a rect, it'd fail to register one it needed.
    if (!plan.fullClear) {
      plan.windows[plan.windowCount++] = notificationRect();
    }
  }

  return plan;
}

void GameRenderer::computeViewOrigin(int playerX, int playerY, int* outViewX, int* outViewY) const {
  planner_.computeViewOrigin(playerX, playerY, buildPlannerLayout(), outViewX, outViewY);
}

game::PlannerLayout GameRenderer::buildPlannerLayout() const {
  game::PlannerLayout layout;
  layout.viewCols = viewCols;
  layout.viewRows = viewRows;
  layout.gridOffsetX = gridOffsetX;
  layout.viewportY = VIEWPORT_Y;
  layout.cellW = CELL_W;
  layout.cellH = CELL_H;
  layout.screenW = screenW;
  layout.statusH = STATUS_H;
  layout.messageY = messageY;
  layout.messageH = messageH;
  return layout;
}

// --- Status Bar ---

void GameRenderer::formatStatusBarText(char hpBuf[24], char mpBuf[24], char depthBuf[16], char lvlBuf[16],
                                       char hungerBuf[16]) const {
  const auto& p = GAME_STATE.player;
  snprintf(hpBuf, 24, "HP:%u/%u", p.hp, game::effectiveMaxHp(p));
  snprintf(mpBuf, 24, "MP:%u/%u", p.mp, p.maxMp);
  snprintf(depthBuf, 16, "Dl:%u", p.dungeonDepth);
  snprintf(lvlBuf, 16, "Cl:%u", p.charLevel);
  // Word-state only, per parent's explicit "no raw counter" call -- the
  // 300/450 thresholds already gate the hunger-clock's own tick outcomes
  // (HungerClock.h), reused here purely for display banding.
  const char* hungerWord = "Fed";
  if (p.hunger >= game::HUNGER_STARVING_THRESHOLD) {
    hungerWord = "Starving";
  } else if (p.hunger >= game::HUNGER_HUNGRY_THRESHOLD) {
    hungerWord = "Hungry";
  }
  snprintf(hungerBuf, 16, "%s", hungerWord);
}

void GameRenderer::drawStatusBar(GfxRenderer& renderer) const {
  char hpBuf[24];
  char mpBuf[24];
  char depthBuf[16];
  char lvlBuf[16];
  char hungerBuf[16];
  formatStatusBarText(hpBuf, mpBuf, depthBuf, lvlBuf, hungerBuf);

  // Left side: HP, MP, hunger word-state
  renderer.drawText(UI_10_FONT_ID, 4, STATUS_Y, hpBuf, true, EpdFontFamily::BOLD);
  int hpWidth = renderer.getTextWidth(UI_10_FONT_ID, hpBuf, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, 4 + hpWidth + 10, STATUS_Y, mpBuf);
  int mpWidth = renderer.getTextWidth(UI_10_FONT_ID, mpBuf);
  renderer.drawText(UI_10_FONT_ID, 4 + hpWidth + 10 + mpWidth + 10, STATUS_Y, hungerBuf);

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

  int viewX = 0;
  int viewY = 0;
  computeViewOrigin(p.x, p.y, &viewX, &viewY);

  // Draw each cell in the viewport
  for (int row = 0; row < viewRows; row++) {
    for (int col = 0; col < viewCols; col++) {
      drawViewportCell(renderer, viewX, viewY, row, col, tiles, fogOfWar, monsters, monsterCount, items, itemCount,
                       visible);
    }
  }
}

void GameRenderer::drawViewportCell(GfxRenderer& renderer, int viewX, int viewY, int row, int col,
                                    const game::Tile* tiles, const uint8_t* fogOfWar, const game::Monster* monsters,
                                    uint8_t monsterCount, const game::Item* items, uint8_t itemCount,
                                    const bool* visible) const {
  const auto& p = GAME_STATE.player;

  int mapY = viewY + row;
  if (mapY < 0 || mapY >= game::MAP_HEIGHT) return;
  int mapX = viewX + col;
  if (mapX < 0 || mapX >= game::MAP_WIDTH) return;

  int screenCellY = VIEWPORT_Y + row * CELL_H;
  int screenCellX = gridOffsetX + col * CELL_W;

  int mapIdx = mapY * game::MAP_WIDTH + mapX;

  bool isExplored = game::fogIsExplored(fogOfWar, mapX, mapY);
  bool isVisible = visible[mapIdx];

  if (!isExplored && !isVisible) {
    // Unseen tile — leave white (cleared screen)
    return;
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
  // Fix 5 (parent-agreed cause 5): word-wrap the message log across up to
  // MESSAGE_LINE_COUNT lines, instead of two hardcoded drawText() calls with
  // no wrap/truncation and no width bound -- long messages used to just run
  // off the right edge of the screen. Wraps only on spaces, never mid-word;
  // overflow continues onto the next line; once wrapped lines exceed the
  // visible window, the oldest lines fall off the top -- the newest message
  // is always fully visible at the bottom.
  constexpr int fontId = UI_10_FONT_ID;
  constexpr int marginX = 4;
  const int maxWidth = screenW - 2 * marginX;
  // Longest line this function will ever hand to getTextWidth()/drawText() --
  // generous enough for any real wrapped line (which is itself bounded by
  // maxWidth) and for the pathological single-overlong-word case below.
  // Stack-only, never heap: this replaces what used to be a std::string
  // built via substr() on every word/line.
  constexpr size_t kLineBufLen = 256;

  // Line references back into GAME_STATE's own message buffers -- no copy of
  // the text itself, just which message and which [pos, pos+len) slice of it.
  // Replaces the old std::string visibleLines[] (which meant a substr()
  // heap allocation per visible line) with plain offsets/lengths.
  struct LineRef {
    int recency;
    size_t pos;
    size_t len;
  };
  LineRef visibleLines[MESSAGE_LINE_COUNT];
  int visibleCount = 0;

  auto pushLine = [&](int recency, size_t pos, size_t len) {
    if (visibleCount < MESSAGE_LINE_COUNT) {
      visibleLines[visibleCount++] = {recency, pos, len};
    } else {
      // Console is full -- drop the oldest visible line to make room for the
      // newer one, same "oldest falls off" behavior as the message ring
      // buffer itself.
      for (int i = 1; i < MESSAGE_LINE_COUNT; i++) visibleLines[i - 1] = visibleLines[i];
      visibleLines[MESSAGE_LINE_COUNT - 1] = {recency, pos, len};
    }
  };

  // Stack buffer reused for every width-check candidate and every drawn line
  // -- copies at most kLineBufLen-1 bytes from the message's own storage, no
  // heap allocation.
  char lineBuf[kLineBufLen];
  auto measureWidth = [&](const char* data, size_t pos, size_t len) -> int {
    size_t n = len < kLineBufLen - 1 ? len : kLineBufLen - 1;
    memcpy(lineBuf, data + pos, n);
    lineBuf[n] = '\0';
    return renderer.getTextWidth(fontId, lineBuf);
  };

  // Walk oldest -> newest so the final visible window always ends on the
  // newest message, with pushLine()'s overflow trim discarding old lines
  // first.
  for (int recency = static_cast<int>(GAME_STATE.messageCount) - 1; recency >= 0; recency--) {
    const std::string& msg = GAME_STATE.getMessage(recency);
    if (msg.empty()) continue;
    const char* data = msg.data();

    size_t pos = 0;
    while (pos < msg.size()) {
      size_t next = pos;
      size_t lineEnd = pos;
      while (next < msg.size()) {
        size_t spacePos = msg.find(' ', next);
        size_t wordEnd = (spacePos == std::string::npos) ? msg.size() : spacePos;
        if (measureWidth(data, pos, wordEnd - pos) > maxWidth) {
          if (lineEnd == pos) {
            // Even the first word alone overflows maxWidth -- never split
            // mid-word, so let it overflow rather than loop forever.
            lineEnd = wordEnd;
          }
          break;
        }
        lineEnd = wordEnd;
        if (spacePos == std::string::npos) break;
        next = spacePos + 1;
      }
      pushLine(recency, pos, lineEnd - pos);
      pos = lineEnd;
      while (pos < msg.size() && msg[pos] == ' ') pos++;
    }
  }

  int lineHeight = renderer.getLineHeight(fontId);
  for (int i = 0; i < visibleCount; i++) {
    const LineRef& ref = visibleLines[i];
    const std::string& msg = GAME_STATE.getMessage(ref.recency);
    size_t n = ref.len < kLineBufLen - 1 ? ref.len : kLineBufLen - 1;
    memcpy(lineBuf, msg.data() + ref.pos, n);
    lineBuf[n] = '\0';
    renderer.drawText(fontId, marginX, messageY + 1 + i * lineHeight, lineBuf);
  }
}

// --- On-Screen Controls (D-Pad + Action/Menu) ---

namespace {
// Draws a simple filled triangle arrow glyph centered in [cx, cy], pointing in the
// given direction. Uses GfxRenderer::fillPolygon (no reusable arrow icon glyph exists
// elsewhere in the codebase — UIIcon only covers file-browser/nav iconography).
enum class ArrowDir { Up, Down, Left, Right };

void drawArrow(GfxRenderer& renderer, int cx, int cy, ArrowDir dir) {
  constexpr int kArrowSize = 16;  // Half-extent of the arrow glyph, in pixels
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

// GfxRenderer::drawLine(x1,y1,x2,y2,lineWidth,state) thickens by offsetting Y
// only (see GfxRenderer.cpp) -- correct for a horizontal run, but it stretches
// a vertical one along its own length instead of widening it. Stroke
// horizontals via the lineWidth overload; stroke verticals as two adjacent
// 1px calls, matching drawRect's own per-side manual inset pattern for its
// vertical sides.
void strokeHLine(GfxRenderer& renderer, int x1, int x2, int y) { renderer.drawLine(x1, y, x2, y, 2, true); }

void strokeVLine(GfxRenderer& renderer, int x, int y1, int y2) {
  renderer.drawLine(x, y1, x, y2, true);
  renderer.drawLine(x + 1, y1, x + 1, y2, true);
}
}  // namespace

void GameRenderer::drawControls(GfxRenderer& renderer) const {
  // Left side: d-pad drawn as one connected cross outline (12-vertex closed
  // polyline), not five bordered cells -- hitTestControls()'s dead corners
  // mean a bordered square would promise four tappable corners that don't
  // exist. No internal divider lines: it must read as a single cross shape.
  const int x0 = 0;
  const int x1 = DPAD_COL_W;
  const int x2 = DPAD_COL_W * 2;
  const int x3 = DPAD_W;
  const int y0 = controlsY;
  const int y1 = controlsY + CONTROL_ROW_H;
  const int y2 = controlsY + CONTROL_ROW_H * 2;
  // One pixel short of controlsY + CONTROL_ROW_H * 3: that sum lands exactly
  // on screenH, and drawPixel() silently drops anything at/past panelHeight
  // -- the bottom arm of the cross would render open. Pulling this in by one
  // row keeps it inside the panel and is invisible to the eye.
  const int y3 = controlsY + CONTROL_ROW_H * 3 - 1;

  strokeHLine(renderer, x1, x2, y0);
  strokeVLine(renderer, x2, y0, y1);
  strokeHLine(renderer, x2, x3, y1);
  strokeVLine(renderer, x3, y1, y2);
  strokeHLine(renderer, x3, x2, y2);
  strokeVLine(renderer, x2, y2, y3);
  strokeHLine(renderer, x2, x1, y3);
  strokeVLine(renderer, x1, y3, y2);
  strokeHLine(renderer, x1, x0, y2);
  strokeVLine(renderer, x0, y2, y1);
  strokeHLine(renderer, x0, x1, y1);
  strokeVLine(renderer, x1, y1, y0);

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
  // A fixed gap separates the buttons from the d-pad (previously butted flush
  // against it with no visual separation), and the border is drawn at the same
  // 2px weight used elsewhere for bordered buttons (GameTitleActivity.cpp:195,
  // ScreenshotUtil.cpp:96) instead of the 1px default. hitTestControls()'s
  // `x < DPAD_W` split is deliberately left untouched -- the gap band still
  // hit-tests as part of the Action/Menu region.
  const int buttonX = DPAD_W + CONTROL_BUTTON_GAP;
  const int buttonW = screenW - buttonX;
  const int buttonH = (CONTROLS_H) / ACTION_MENU_BUTTON_COUNT;

  static constexpr StrId kButtonLabelIds[ACTION_MENU_BUTTON_COUNT] = {
      StrId::STR_DM_HINT_ACTION,
      StrId::STR_DM_HINT_MENU,
  };

  for (int i = 0; i < ACTION_MENU_BUTTON_COUNT; i++) {
    const int buttonY = controlsY + i * buttonH;
    renderer.drawRect(buttonX, buttonY, buttonW, buttonH, 2, true);

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
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y, isVictory ? tr(STR_DM_VICTORY) : tr(STR_DM_YOU_DIED), true,
                            EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX, boxW);
  y += 34;

  char line[64];
  if (!isVictory) {
    snprintf(line, sizeof(line), "%s %s", tr(STR_DM_DEATH_CAUSE), data.cause);
    renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO,
                              boxX, boxW);
    y += 26;
  }

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_FLOOR), data.floor);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX,
                            boxW);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_TURNS), data.turns);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX,
                            boxW);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_KILLS), data.kills);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX,
                            boxW);
  y += 22;

  snprintf(line, sizeof(line), "%s %u", tr(STR_DM_STAT_LEVEL), data.level);
  renderer.drawCenteredText(UI_12_FONT_ID, y, line, true, EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX,
                            boxW);
  y += 30;

  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_DM_ACHIEVEMENTS), true, EpdFontFamily::REGULAR,
                            BidiUtils::BidiBaseDir::AUTO, boxX, boxW);
  y += 22;

  if (data.unlockedCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_DM_NO_ACHIEVEMENTS), true, EpdFontFamily::REGULAR,
                              BidiUtils::BidiBaseDir::AUTO, boxX, boxW);
  } else {
    for (uint8_t i = 0; i < data.unlockedCount; i++) {
      renderer.drawCenteredText(UI_10_FONT_ID, y, game::achievementShortName(data.unlockedIds[i]), true,
                                EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX, boxW);
      y += 18;
    }
  }

  renderer.drawCenteredText(UI_10_FONT_ID, boxY + boxH - 24, tr(STR_DM_TAP_TO_CONTINUE), true,
                            EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, boxX, boxW);

  // One-shot full-refresh: new high-contrast content over a stale buffer
  // needs a clean waveform, not the periodic ghost-guard cadence draw() uses.
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}

Rect GameRenderer::corruptNoticeContinueRect() const {
  // Mirrors the box geometry drawCorruptSaveNotice() computes -- kept in one
  // place so the drawn button and its touch region can never drift apart.
  const int boxW = screenW - 60;
  const int boxH = 340;
  const int boxX = (screenW - boxW) / 2;
  const int boxY = (screenH - boxH) / 2;

  // Same running-y layout drawCorruptSaveNotice() uses to reach the option
  // row area: title (+53), body (+bodyH+10). The single Continue button is
  // centered in the footprint the old two-row Purge/Leave layout occupied, so
  // the title/body spacing Pixel measured above it is untouched.
  const int bodyH = 150;
  const int optionAreaY = boxY + 26 + 53 + bodyH + 10;
  const int optionAreaH = 76;  // old two-row footprint: 2 * (optionH=34 + 8) - 8
  const int continueH = 34;
  const int continueMarginX = 40;

  return Rect(boxX + continueMarginX, optionAreaY + (optionAreaH - continueH) / 2, boxW - 2 * continueMarginX,
             continueH);
}

bool GameRenderer::hitTestCorruptSaveNoticeContinue(int x, int y) const {
  const Rect r = corruptNoticeContinueRect();
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

void GameRenderer::drawCorruptSaveNotice(GfxRenderer& renderer, bool wholeRun, uint8_t depth) const {
  // Same overlay discipline as drawEndScreen(): no clearScreen(), self-contained
  // box, one-shot FULL_REFRESH -- this is the blocking corrupt-save notice
  // (Phase 12/13), not the fixed single-dismiss showNotification() system,
  // since it explains itself before continuing rather than just naming an event.
  const int boxW = screenW - 60;
  const int boxH = 340;
  const int boxX = (screenW - boxW) / 2;
  const int boxY = (screenH - boxH) / 2;

  renderer.fillRoundedRect(boxX, boxY, boxW, boxH, 8, Color::White);
  renderer.drawRoundedRect(boxX, boxY, boxW, boxH, 2, 8, true);

  int y = boxY + 26;
  // Centre on the box, not the full screen -- and shrink/truncate rather than
  // let an overlong title paint past the box edges (same truncatedText()
  // pattern ConfirmationActivity.cpp uses for its heading).
  const int titleMarginX = 24;
  const int titleMaxWidth = boxW - 2 * titleMarginX;
  std::string titleText =
      renderer.truncatedText(NOTOSANS_18_FONT_ID, tr(STR_DM_CORRUPT_NOTICE_TITLE), titleMaxWidth);
  Rect titleBounds(boxX, y, boxW, 30);
  UITheme::drawCenteredText(renderer, titleBounds, NOTOSANS_18_FONT_ID, y, titleText.c_str(), true);
  // +13px beyond the base 40px gap, per Pixel's exact 1:1 pixel measurement (msg 3876):
  // title ink bottom to body first-line ink top was 3px (tighter than the body's own
  // 5px internal line pitch, reading as one dense block); target gap is 16px, and since
  // everything below is positioned off this running y, a single +13px here carries the
  // shift through the body and both option rows without disturbing their own spacing.
  y += 53;

  char body[192];
  if (wholeRun) {
    // Whole-run rejection: no floor number to report, save.bin itself failed
    // to parse -- a distinct body string, not the per-level one with its
    // depth substitution left blank/zero.
    snprintf(body, sizeof(body), "%s", tr(STR_DM_CORRUPT_NOTICE_BODY_WHOLERUN));
  } else {
    snprintf(body, sizeof(body), tr(STR_DM_CORRUPT_NOTICE_BODY), static_cast<unsigned>(depth));
  }

  const int bodyMarginX = 24;
  const int bodyH = 150;
  Rect bodyBounds(boxX + bodyMarginX, y, boxW - 2 * bodyMarginX, bodyH);
  UITheme::drawCenteredWrappedText(renderer, bodyBounds, UI_12_FONT_ID, body, /*maxLines=*/6, true,
                                   EpdFontFamily::REGULAR, UITheme::TextVerticalAlignment::TOP);
  y += bodyH + 10;

  // Single Continue button -- no second option, no selection to toggle
  // (Stuart's locked spec, msg 3940). Filled black, same inverted-fill visual
  // language the old highlighted row used, so it reads unambiguously as a
  // tappable affordance rather than plain text. Geometry shared with
  // hitTestCorruptSaveNoticeContinue() via corruptNoticeContinueRect() so the
  // drawn button and its touch region can never drift apart.
  const Rect continueRect = corruptNoticeContinueRect();
  renderer.fillRoundedRect(continueRect.x, continueRect.y, continueRect.width, continueRect.height, 4, Color::Black);
  // Baseline offset matches the old option row's Pixel-measured -15 (descenders
  // clear the fill bar).
  renderer.drawCenteredText(UI_12_FONT_ID, continueRect.y + continueRect.height / 2 - 15,
                            tr(STR_DM_CORRUPT_NOTICE_CONTINUE), /*black=*/false, EpdFontFamily::REGULAR,
                            BidiUtils::BidiBaseDir::AUTO, continueRect.x, continueRect.width);

  // One-shot full-refresh: same reasoning as drawEndScreen() -- new
  // high-contrast content over a stale buffer needs a clean waveform.
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}

// Title string per NotificationKind (Phase 9 work item 3, requirement 3 --
// flash-resident lookup, never per-turn constructed). Indexed directly by
// the enum, so any future NotificationKind addition that forgets an entry
// here fails loudly at compile time via the array-size mismatch below.
static constexpr StrId kNotificationTitleId[] = {
    StrId::STR_DM_NOTIF_LEVEL_UP,
    StrId::STR_DM_NOTIF_ACHIEVEMENT,
    StrId::STR_DM_NOTIF_FLOOR_ENTRY,
    StrId::STR_DM_NOTIF_BOSS_ARRIVAL,
    StrId::STR_DM_NOTIF_DEATH,
};

void GameRenderer::showNotification(NotificationKind kind, const char* body) {
  notificationActive_ = true;
  notificationKind_ = kind;
  snprintf(notificationBody_, NOTIFICATION_BODY_LEN, "%s", body);
  notificationDirty_ = true;
}

void GameRenderer::showAchievementNotification(game::AchievementId id) {
  notificationActive_ = true;
  notificationKind_ = NotificationKind::Achievement;
  snprintf(notificationBody_, NOTIFICATION_BODY_LEN, "%s", game::achievementShortName(id));
  const game::AchievementDef& def = game::ACHIEVEMENT_DEFS[static_cast<uint8_t>(id)];
  game::achievementRewardText(def, notificationRewardBody_, NOTIFICATION_BODY_LEN);
  notificationDirty_ = true;
}

void GameRenderer::dismissNotification() {
  if (!notificationActive_) return;
  notificationActive_ = false;
  notificationDirty_ = true;
}

DirtyWindow GameRenderer::notificationRect() const {
  // Fixed position in the reserved banner band (see BANNER_H/bannerY),
  // full-width minus a side margin -- depends only on screen layout
  // (computeLayout() output), never on notification content, so draw()'s
  // partial path and planFrame()'s FramePlan-building code always agree on
  // the same rect without sharing any extra state.
  DirtyWindow w;
  w.kind = DirtyWindow::Kind::Notification;
  w.x = NOTIFICATION_MARGIN_X;
  w.y = bannerY;
  w.w = screenW - 2 * NOTIFICATION_MARGIN_X;
  w.h = BANNER_H;
  return w;
}

// Descending-size candidate list for drawAchievementBanner()'s largest-fit
// search (Stuart's "just big text" call, msg 4082) -- ordered largest-first
// so the first candidate that fits IS the largest that fits, no second pass
// needed to confirm it. NOTOSANS_18 tops the list since it's the biggest
// font this codebase has (see fontIds.h); SMALL_FONT_ID anchors the bottom
// as the last-resort fallback -- realistic achievement names fit well before
// reaching it, but if every candidate somehow failed to fit, the loop below
// still leaves fontId on SMALL_FONT_ID (the smallest, hence most-likely-to-
// fit) rather than an earlier, worse-fitting candidate.
static constexpr int kAchievementFontCandidates[] = {
    NOTOSANS_18_FONT_ID, NOTOSERIF_18_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSERIF_16_FONT_ID,
    NOTOSANS_14_FONT_ID, NOTOSERIF_14_FONT_ID, UI_12_FONT_ID,       NOTOSANS_12_FONT_ID,
    NOTOSERIF_12_FONT_ID, UI_10_FONT_ID,        SMALL_FONT_ID,
};

void GameRenderer::drawAchievementBanner(GfxRenderer& renderer, const DirtyWindow& rect) const {
  // Same horizontal box drawCenteredText's own clamp already assumes
  // (rect.x/rect.w passed straight through as boxX/boxWidth below) -- no
  // separate margin math here, so there's exactly one source of "how wide is
  // available" for both the fit search and the draw.
  constexpr EpdFontFamily::Style kStyle = EpdFontFamily::BOLD;
  int fontId = kAchievementFontCandidates[0];
  int lineH = renderer.getLineHeight(fontId);
  for (int candidate : kAchievementFontCandidates) {
    fontId = candidate;
    lineH = renderer.getLineHeight(fontId);
    // The SAME getTextWidth() call drawCenteredText() itself uses internally
    // to center -- deliberate (parent msg 4082: "whatever measures the text
    // before centring it must be the same call that draws it"). Picking a
    // font here that this exact call reports as fitting means
    // drawCenteredText()'s own internal truncatedText() safety clamp can
    // never actually trigger for a real achievement name. The lineH <= rect.h
    // check is the vertical half of the same guarantee -- without it, a tall
    // candidate could still be "picked" on width alone and then centeredY
    // below could land outside the rect (and, since rect.y/rect.h are
    // themselves proven inside the panel by notificationRect()'s layout
    // invariant, outside the rect is the only way this text could ever
    // reach panelHeight and get silently dropped by drawPixel).
    if (renderer.getTextWidth(fontId, notificationBody_, kStyle, BidiUtils::BidiBaseDir::AUTO) <= rect.w &&
        lineH <= rect.h) {
      break;
    }
  }

  const int centeredY = rect.y + (rect.h - lineH) / 2;
  renderer.drawCenteredText(fontId, centeredY, notificationBody_, true, kStyle, BidiUtils::BidiBaseDir::AUTO, rect.x,
                            rect.w);
}

void GameRenderer::drawNotification(GfxRenderer& renderer) const {
  const DirtyWindow rect = notificationRect();

  // Achievement unlocks render as bare centered text on the existing band --
  // no box, no title bar, nothing else in this function applies to them.
  if (notificationKind_ == NotificationKind::Achievement) {
    drawAchievementBanner(renderer, rect);
    return;
  }

  renderer.fillRoundedRect(rect.x, rect.y, rect.w, rect.h, 6, Color::White);
  renderer.drawRoundedRect(rect.x, rect.y, rect.w, rect.h, 2, 6, true);

  // Inverted (black-filled) title bar across the top of the box, white
  // centered text -- the "SYSTEM:" boxed-notification look the spec calls
  // for, reusing the same primitives drawEndScreen() already proved out.
  renderer.fillRect(rect.x, rect.y, rect.w, NOTIFICATION_TITLE_H, true);
  const char* title = I18n::getInstance().get(kNotificationTitleId[static_cast<uint8_t>(notificationKind_)]);
  renderer.drawCenteredText(UI_12_FONT_ID, rect.y + 6, title, false, EpdFontFamily::REGULAR,
                            BidiUtils::BidiBaseDir::AUTO, rect.x, rect.w);

  // Body sits directly under the title bar inside the fixed BANNER_H band --
  // title (NOTIFICATION_TITLE_H) + this gap + one body line must fit inside
  // BANNER_H, same box-clamp discipline as every other drawCenteredText call
  // site now follows (rect.x/rect.w clamp already applied below).
  const int bodyTop = rect.y + NOTIFICATION_TITLE_H + 4;
  renderer.drawCenteredText(UI_10_FONT_ID, bodyTop, notificationBody_, true,
                            EpdFontFamily::REGULAR, BidiUtils::BidiBaseDir::AUTO, rect.x, rect.w);
}
