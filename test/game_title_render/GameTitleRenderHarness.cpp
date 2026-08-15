// Standalone host harness (no gtest, no CMake wiring -- compiled directly with g++), same
// technique as test/game_save/GameSaveRoundTripHarness.cpp: compiles the REAL, unmodified
// GfxRenderer.cpp / Activity.cpp / GameTitleActivity.cpp / font pipeline against host stub
// headers (test/game_title_render/stubs/) shadowing only what the Arduino/FreeRTOS/SPI
// coupling demands. Renders the real GameTitleActivity::render() draw calls against the
// real font assets into an in-memory 480x800 1bpp framebuffer (test/game_title_render/
// stubs/HalDisplay.h) and dumps it to a PGM.
//
// Build:
//   g++ -std=c++20 -O2 -Wall -Wextra \
//     -I test/game_title_render/stubs -I src -I src/activities -I src/activities/game \
//     -I lib/GfxRenderer -I lib/EpdFont -I lib/EpdFont/builtinFonts -I lib/Serialization \
//     -I src/game \
//     test/game_title_render/GameTitleRenderHarness.cpp \
//     lib/GfxRenderer/GfxRenderer.cpp \
//     src/activities/Activity.cpp \
//     src/activities/game/GameTitleActivity.cpp \
//     -o /tmp/game_title_render_harness
// Run: /tmp/game_title_render_harness /tmp/game_title.pgm

#include <HalDisplay.h>

#include <builtinFonts/notosans_16_regular.h>
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#include <cstdio>
#include <cstdlib>

#include "EpdFont.h"
#include "EpdFontFamily.h"
#include "FontCacheManager.h"
#include "FontDecompressor.h"
#include "GameTitleActivity.h"
#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "RenderLock.h"
#include "fontIds.h"

// Logical portrait canvas GameTitleActivity draws into. GfxRenderer's Portrait orientation
// rotates these into the physical 800x480 panel buffer -- see HalDisplay.h's comment.
static constexpr int kLogicalWidth = 480;
static constexpr int kLogicalHeight = 800;

int main(int argc, char** argv) {
  const char* outPath = argc > 1 ? argv[1] : "/tmp/game_title.pgm";

  GfxRenderer renderer(display);
  renderer.begin();  // real main.cpp calls this right after construction (src/main.cpp:242) --
                      // sets frameBuffer/panelWidth/panelHeight/panelWidthBytes from the
                      // display; omitting it left frameBuffer null, causing the drawPixel SEGV.

  // Register the real, unmodified builtin fonts GameTitleActivity actually draws with
  // (UI_10/UI_12/NOTOSANS_16 -- see its kHudFontId/kCertFontId/kTapFontId/kAttributionFontId),
  // exactly the way src/main.cpp's setupDisplayAndFonts() does. Both REGULAR and BOLD faces are
  // needed for UI_10/UI_12 -- the cert line (item 7) and HUD blocks (items 3 & 8) draw with
  // EpdFontFamily::BOLD, and main.cpp registers a real bold face for both (ubuntu_10_bold,
  // ubuntu_12_bold), not a synthesized/regular-fallback bold. An earlier version of this harness
  // only registered REGULAR, so every BOLD draw silently measured/rendered the wrong glyphs.
  static EpdFont ui10RegularFont(&ubuntu_10_regular);
  static EpdFont ui10BoldFont(&ubuntu_10_bold);
  static EpdFontFamily ui10FontFamily(&ui10RegularFont, &ui10BoldFont);
  static EpdFont ui12RegularFont(&ubuntu_12_regular);
  static EpdFont ui12BoldFont(&ubuntu_12_bold);
  static EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);
  static EpdFont notosans16RegularFont(&notosans_16_regular);
  static EpdFontFamily notosans16FontFamily(&notosans16RegularFont);
  static EpdFont smallFont(&notosans_8_regular);
  static EpdFontFamily smallFontFamily(&smallFont);
  renderer.insertFont(UI_10_FONT_ID, ui10FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);
  renderer.insertFont(NOTOSANS_16_FONT_ID, notosans16FontFamily);
  renderer.insertFont(SMALL_FONT_ID, smallFontFamily);

  // These builtin fonts store compressed glyph data (fontData->groups != nullptr) --
  // GfxRenderer::getGlyphBitmap() (GfxRenderer.cpp:60-72) requires a FontDecompressor reachable
  // via fontCacheManager_->getDecompressor() to resolve any glyph, else every glyph bitmap
  // silently comes back null. Wire it up exactly as src/main.cpp's setupDisplayAndFonts() does.
  FontDecompressor fontDecompressor;
  if (!fontDecompressor.init()) {
    std::fprintf(stderr, "font decompressor init failed\n");
    return 1;
  }
  FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts());
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);

  HalGPIO localGpio;
  MappedInputManager mappedInput(localGpio, renderer);

  GameTitleActivity activity(renderer, mappedInput);
  activity.render(RenderLock{});

  // Dump the real in-memory framebuffer as a PGM (P5, 8-bit grayscale) -- no PNG encoder
  // needed per explicit scope. The framebuffer is stored in PHYSICAL panel layout (800x480,
  // rotated 90 degrees by GfxRenderer's Portrait orientation), so un-rotate back to the
  // logical 480x800 portrait canvas GameTitleActivity actually drew to, using the inverse of
  // rotateCoordinates()'s Portrait case (phyX=y, phyY=panelHeight-1-x -> x=panelHeight-1-phyY,
  // y=phyX), so the output PGM reads right-side-up rather than sideways.
  FILE* f = std::fopen(outPath, "wb");
  if (!f) {
    std::fprintf(stderr, "failed to open %s for write\n", outPath);
    return 1;
  }
  std::fprintf(f, "P5\n%d %d\n255\n", kLogicalWidth, kLogicalHeight);
  const uint8_t* fb = display.getFrameBuffer();
  const int physWidthBytes = HalDisplay::DISPLAY_WIDTH_BYTES;
  const int physHeight = HalDisplay::DISPLAY_HEIGHT;
  for (int y = 0; y < kLogicalHeight; y++) {
    for (int x = 0; x < kLogicalWidth; x++) {
      int phyX = y;
      int phyY = physHeight - 1 - x;
      int byteIdx = phyY * physWidthBytes + (phyX / 8);
      int bit = 7 - (phyX % 8);
      bool set = (fb[byteIdx] >> bit) & 1;  // 1 = white per clearScreen(0xFF) convention
      uint8_t px = set ? 255 : 0;
      std::fputc(px, f);
    }
  }
  std::fclose(f);
  std::fprintf(stderr, "wrote %s (%dx%d)\n", outPath, kLogicalWidth, kLogicalHeight);
  return 0;
}
