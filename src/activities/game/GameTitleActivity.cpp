#include "GameTitleActivity.h"

#include <GfxRenderer.h>

#include <cmath>

#include "GameActivity.h"
#include "MappedInputManager.h"
#include "fontIds.h"
#include "game/GameState.h"

// --- Pixel-art block letters (5 wide x 7 tall bitmaps) ---

namespace {

// Each letter is 7 rows of 5-bit patterns (MSB = leftmost pixel)
// clang-format off
constexpr uint8_t GLYPH_D[] = {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
constexpr uint8_t GLYPH_E[] = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
constexpr uint8_t GLYPH_N[] = {0b10001, 0b11001, 0b10101, 0b10101, 0b10011, 0b10001, 0b10001};
constexpr uint8_t GLYPH_LETTER_W[] = {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001};
constexpr uint8_t GLYPH_O[] = {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
constexpr uint8_t GLYPH_R[] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001};
constexpr uint8_t GLYPH_L[] = {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111};
constexpr uint8_t GLYPH_U[] = {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110};
constexpr uint8_t GLYPH_G[] = {0b01111, 0b10000, 0b10000, 0b10011, 0b10001, 0b10001, 0b01111};
// clang-format on

constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 7;

void drawBlockLetter(GfxRenderer& renderer, const uint8_t* glyph, int originX, int originY, int scale) {
  for (int row = 0; row < GLYPH_H; row++) {
    for (int col = 0; col < GLYPH_W; col++) {
      if (glyph[row] & (1 << (GLYPH_W - 1 - col))) {
        renderer.fillRect(originX + col * scale, originY + row * scale, scale - 1, scale - 1);
      }
    }
  }
}

struct LetterEntry {
  const uint8_t* glyph;
};

void drawWord(GfxRenderer& renderer, const LetterEntry* letters, int count, int centerX, int originY, int scale) {
  int letterW = GLYPH_W * scale + scale;  // Letter width + gap
  int totalW = count * letterW - scale;   // No trailing gap
  int startX = centerX - totalW / 2;

  for (int i = 0; i < count; i++) {
    drawBlockLetter(renderer, letters[i].glyph, startX + i * letterW, originY, scale);
  }
}

// Converts a baseline y (Pixel's spec convention) to the top-edge y drawText/drawCenteredText
// expect — GfxRenderer.cpp:569 adds the font's ascender to the y passed in before using it as
// the glyph baseline, so passing a baseline straight through double-applies the ascent.
int baselineToTopY(const GfxRenderer& renderer, int fontId, int baselineY) {
  return baselineY - renderer.getFontAscenderSize(fontId);
}

// Corner tick plus-mark: two crossing strokes centered on (cx, cy), each arm reaching
// armSpan px out from center in both directions.
void drawCornerTick(GfxRenderer& renderer, int cx, int cy, int armSpan, int strokeWidth) {
  renderer.drawLine(cx - armSpan, cy, cx + armSpan, cy, strokeWidth, true);
  renderer.drawLine(cx, cy - armSpan, cx, cy + armSpan, strokeWidth, true);
}

// Full circle outline built from 4 quadrant arcs (GfxRenderer::drawArc only draws one
// quadrant per call, selected by xDir/yDir in {-1,1}).
void drawCircleOutline(GfxRenderer& renderer, int radius, int cx, int cy, int strokeWidth) {
  renderer.drawArc(radius, cx, cy, 1, 1, strokeWidth, true);
  renderer.drawArc(radius, cx, cy, 1, -1, strokeWidth, true);
  renderer.drawArc(radius, cx, cy, -1, 1, strokeWidth, true);
  renderer.drawArc(radius, cx, cy, -1, -1, strokeWidth, true);
}

// 24 radial ticks around (cx, cy) every 15 degrees, each running from rInner to rOuter.
void drawTickRing(GfxRenderer& renderer, int cx, int cy, int rInner, int rOuter, int strokeWidth) {
  constexpr int kTickCount = 24;
  for (int i = 0; i < kTickCount; i++) {
    const float angle = static_cast<float>(i) * (static_cast<float>(M_PI) / 12.0f);  // i * 15 deg
    const int x1 = cx + static_cast<int>(std::lround(std::cos(angle) * rInner));
    const int y1 = cy + static_cast<int>(std::lround(std::sin(angle) * rInner));
    const int x2 = cx + static_cast<int>(std::lround(std::cos(angle) * rOuter));
    const int y2 = cy + static_cast<int>(std::lround(std::sin(angle) * rOuter));
    renderer.drawLine(x1, y1, x2, y2, strokeWidth, true);
  }
}

}  // namespace

// --- Lifecycle ---

void GameTitleActivity::onEnter() {
  Activity::onEnter();
  rendered = false;
  requestUpdate();
}

void GameTitleActivity::loop() {
  if (!rendered) return;

  int tx, ty;
  // Tap-anywhere start: wasScreenTapped() only fires on the frame the physical touch is
  // released, and the main loop always re-polls gpio (clearing that release edge) before this
  // activity's replacement gets its own first loop() call — so the tap that starts the game
  // cannot be re-read as a touch by GameActivity's control surface on the next frame.
  if (mappedInput.wasAnyReleased() || mappedInput.wasScreenTapped(tx, ty)) {
    // Start (or resume) the run. GameActivity::onEnter() itself checks for a save file and
    // loads it, so a fresh game only needs to be seeded here when no save exists.
    if (!GAME_STATE.hasSaveFile()) {
      GAME_STATE.newGame(static_cast<uint32_t>(millis()) ^ 0xDEADBEEFu);
    }
    activityManager.replaceActivity(std::make_unique<GameActivity>(renderer, mappedInput));
  }
}

// --- Rendering ---

void GameTitleActivity::render(RenderLock&&) {
  const auto pageWidth = renderer.getScreenWidth();
  const int centerX = pageWidth / 2;

  // Font substitutions: this codebase has no mono, condensed, 15px, or 38px font asset
  // (confirmed against src/fontIds.h and lib/EpdFont/builtinFonts/) — Pixel's spec calls for
  // all four. Nearest available stand-ins, flagged back to her:
  //   mono bold 15px  (HUD blocks, items 3 & 8) -> UI_12_FONT_ID, BOLD
  //   mono bold 12px  (cert line, item 7)       -> UI_12_FONT_ID, BOLD (exact size match)
  //   mono regular 16px (tap prompt, item 9)    -> NOTOSANS_16_FONT_ID, REGULAR
  //   mono regular 10px (attribution, item 10)  -> UI_10_FONT_ID, REGULAR
  //   bold condensed sans 38px (wordmark, item 6) -> font-independent pixel block letters
  //     (drawWord/drawBlockLetter below), not a font call at all.
  constexpr int kHudFontId = UI_12_FONT_ID;
  constexpr int kCertFontId = UI_12_FONT_ID;
  constexpr int kTapFontId = NOTOSANS_16_FONT_ID;
  constexpr int kAttributionFontId = UI_10_FONT_ID;

  // White background (default)
  renderer.clearScreen();

  // 1. Safe-area frame
  renderer.drawRect(20, 20, 440, 760, 3, true);

  // 2. Corner ticks (plus-marks, 17px arm span, 4px stroke)
  drawCornerTick(renderer, 20, 20, 17, 4);
  drawCornerTick(renderer, 460, 20, 17, 4);
  drawCornerTick(renderer, 20, 780, 17, 4);
  drawCornerTick(renderer, 460, 780, 17, 4);

  // 3. Top HUD block (mono bold 15px stand-in, left-aligned x=40)
  renderer.drawText(kHudFontId, 40, baselineToTopY(renderer, kHudFontId, 100), "FLOOR ---- 01", true,
                     EpdFontFamily::BOLD);
  renderer.drawText(kHudFontId, 40, baselineToTopY(renderer, kHudFontId, 124), "STATUS --- DESCENDING", true,
                     EpdFontFamily::BOLD);
  renderer.drawText(kHudFontId, 40, baselineToTopY(renderer, kHudFontId, 148), "AUDIENCE - 4.2M", true,
                     EpdFontFamily::BOLD);

  // 4. Broadcast seal (two concentric circles, center 240,420)
  drawCircleOutline(renderer, 110, 240, 420, 3);
  drawCircleOutline(renderer, 130, 240, 420, 3);

  // 5. Tick ring (24 ticks every 15 degrees, r=124 to r=144)
  drawTickRing(renderer, 240, 420, 124, 144, 3);

  // 6. Wordmark stamp: white-clear rect, outer/inner outline, block-letter text
  renderer.fillRect(20, 365, 440, 110, false);
  renderer.drawRect(40, 381, 400, 78, 3, true);
  renderer.drawRect(48, 388, 385, 64, 2, true);
  constexpr int kWordmarkScale = 5;
  const LetterEntry world[] = {{GLYPH_LETTER_W}, {GLYPH_O}, {GLYPH_R}, {GLYPH_L}, {GLYPH_D}};
  drawWord(renderer, world, 5, centerX, 400, kWordmarkScale);
  const LetterEntry dungeon[] = {{GLYPH_D}, {GLYPH_U}, {GLYPH_N}, {GLYPH_G}, {GLYPH_E}, {GLYPH_O}, {GLYPH_N}};
  drawWord(renderer, dungeon, 7, centerX, 435, kWordmarkScale);

  // 7. Certification line (mono bold 12px stand-in)
  renderer.drawCenteredText(kCertFontId, baselineToTopY(renderer, kCertFontId, 494), "LETHALITY UNRATED", true,
                             EpdFontFamily::BOLD);

  // 8. Bottom HUD block (mono bold 15px stand-in)
  // LIVE status-light bullet, center (45,605) r=5 -> bounding box (40,600) 10x10, fully rounded
  renderer.fillRoundedRect(40, 600, 10, 10, 5, Color::Black);
  renderer.drawText(kHudFontId, 58, baselineToTopY(renderer, kHudFontId, 610), "SIGNAL --- LIVE", true,
                     EpdFontFamily::BOLD);
  renderer.drawText(kHudFontId, 52, baselineToTopY(renderer, kHudFontId, 634), "EXIT ------ DENIED", true,
                     EpdFontFamily::BOLD);
  renderer.drawText(kHudFontId, 52, baselineToTopY(renderer, kHudFontId, 658), "SPONSOR --- UNKNOWN", true,
                     EpdFontFamily::BOLD);

  // 9. Tap prompt (mono regular 16px stand-in)
  renderer.drawCenteredText(kTapFontId, baselineToTopY(renderer, kTapFontId, 740), "[ tap anywhere to continue ]");

  // 10. Attribution (mono regular 10px stand-in)
  renderer.drawCenteredText(kAttributionFontId, baselineToTopY(renderer, kAttributionFontId, 756),
                             "Inspired by Moria & Angband");

  renderer.displayBuffer();
  rendered = true;
}
