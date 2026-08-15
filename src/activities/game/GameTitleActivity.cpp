#include "GameTitleActivity.h"

#include <GfxRenderer.h>

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
  const auto pageHeight = renderer.getScreenHeight();
  const int centerX = pageWidth / 2;

  // White background (default)
  renderer.clearScreen();

  // Draw a decorative double border
  renderer.drawRect(10, 10, pageWidth - 20, pageHeight - 20);
  renderer.drawRect(13, 13, pageWidth - 26, pageHeight - 26);

  // Flavor text
  renderer.drawCenteredText(UI_10_FONT_ID, 40, "A roguelike for the CrossPoint Reader");

  // Separator line
  renderer.drawLine(40, 70, pageWidth - 40, 70);

  // Block letter title: "WORLD"
  constexpr int SCALE = 8;
  const LetterEntry world[] = {{GLYPH_LETTER_W}, {GLYPH_O}, {GLYPH_R}, {GLYPH_L}, {GLYPH_D}};
  drawWord(renderer, world, 5, centerX, 90, SCALE);

  // Block letter title: "DUNGEON"
  const LetterEntry dungeon[] = {{GLYPH_D}, {GLYPH_U}, {GLYPH_N}, {GLYPH_G}, {GLYPH_E}, {GLYPH_O}, {GLYPH_N}};
  drawWord(renderer, dungeon, 7, centerX, 160, SCALE);

  // Separator line
  renderer.drawLine(40, 230, pageWidth - 40, 230);

  // Subtitle
  renderer.drawCenteredText(UI_10_FONT_ID, 250, "W O R L D    D U N G E O N", true, EpdFontFamily::BOLD);

  // Decorative pickaxe symbol
  renderer.drawCenteredText(UI_10_FONT_ID, 290, "--- * ---");

  // Version
  renderer.drawCenteredText(UI_10_FONT_ID, 340, "v0.1.0");

  // Credits
  renderer.drawCenteredText(SMALL_FONT_ID, 400, "Inspired by Moria & Angband");

  // Separator
  renderer.drawLine(40, 460, pageWidth - 40, 460);

  // Goal hint
  renderer.drawCenteredText(SMALL_FONT_ID, 500, "Descend 26 levels. Defeat the Necromancer.");
  renderer.drawCenteredText(SMALL_FONT_ID, 525, "Claim the Ring of Power.");

  // Press any key prompt
  renderer.drawCenteredText(UI_10_FONT_ID, 740, "[ tap anywhere to continue ]");

  renderer.displayBuffer();
  rendered = true;
}
