// Standalone host harness (no gtest, no CMake wiring -- compiled directly with g++), same
// technique as test/game_title_render/GameTitleRenderHarness.cpp. Compiles the REAL, unmodified
// GameRenderer.cpp (specifically drawCorruptSaveNotice()) and the REAL, unmodified
// UITheme.cpp/UITheme.h (specifically drawCenteredWrappedText()/drawCenteredText(), the wrapping
// code actually under test) against host stub headers, and dumps the resulting framebuffer to a
// PGM image.
//
// Per parent's explicit instruction (msg 3868, 2026-08-16): the real UITheme.h and every line of
// drawCenteredWrappedText are untouched. UITheme.cpp links because UITheme's real, unmodified
// constructor is satisfied by empty out-of-line method stubs for the four theme classes
// (test/corrupt_save_notice_render/stub_themes/*.cpp) -- those stubbed bodies are never invoked
// by drawCorruptSaveNotice()'s tested path (it only calls the two static wrapping/centering
// methods), so nothing under test is shadowed. See test/corrupt_save_notice_render/README.
//
// GameRenderer has zero Activity/ActivityManager coupling (unlike GameTitleActivity) -- no
// mirror/ symlink tree is needed here at all.

#include <HalDisplay.h>

#include <builtinFonts/notosans_18_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>

#include <cstdio>
#include <cstdlib>

#include "EpdFont.h"
#include "EpdFontFamily.h"
#include "FontCacheManager.h"
#include "FontDecompressor.h"
#include "GameRenderer.h"
#include "GfxRenderer.h"
#include "fontIds.h"

// Logical portrait canvas the game screens draw into -- same convention as
// test/game_title_render/GameTitleRenderHarness.cpp.
static constexpr int kLogicalWidth = 480;
static constexpr int kLogicalHeight = 800;

int main(int argc, char** argv) {
  const char* outPath = argc > 1 ? argv[1] : "/tmp/corrupt_save_notice.pgm";
  // depth/selection are harness-only inputs (not read from real save data) -- default to a
  // representative mid-run floor number and the Purge (default-highlighted) selection, matching
  // the approved notice copy's "Floor %u" placeholder and GameRenderer.h's documented
  // selection=0-is-Purge/1-is-Leave contract.
  const unsigned depth = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : 7;
  const uint8_t selection = argc > 3 ? static_cast<uint8_t>(std::atoi(argv[3])) : 0;
  // wholeRun defaults to false (per-level notice, the original behavior this harness was
  // built for) -- pass a 4th arg of 1 to render the whole-run/no-floor-number variant instead.
  const bool wholeRun = argc > 4 ? std::atoi(argv[4]) != 0 : false;

  GfxRenderer renderer(display);
  renderer.begin();  // sets frameBuffer/panelWidth/panelHeight from the display -- required
                      // before any draw call, same as GameTitleRenderHarness.cpp.

  // Register the real, unmodified builtin fonts drawCorruptSaveNotice() actually draws with:
  // NOTOSANS_18_FONT_ID for the title (drawCenteredText), UI_12_FONT_ID for the wrapped body
  // and the two option labels. Matching src/main.cpp's setupDisplayAndFonts() exactly -- UI_12
  // is backed by the ubuntu_12 asset (not notosans_12), regular+bold (drawCorruptSaveNotice()
  // itself only ever requests REGULAR, but registering both faces matches main.cpp exactly and
  // costs nothing).
  static EpdFont notosans18RegularFont(&notosans_18_regular);
  static EpdFontFamily notosans18FontFamily(&notosans18RegularFont);
  static EpdFont ui12RegularFont(&ubuntu_12_regular);
  static EpdFont ui12BoldFont(&ubuntu_12_bold);
  static EpdFontFamily ui12FontFamily(&ui12RegularFont, &ui12BoldFont);
  renderer.insertFont(NOTOSANS_18_FONT_ID, notosans18FontFamily);
  renderer.insertFont(UI_12_FONT_ID, ui12FontFamily);

  // Required or every glyph bitmap comes back null silently (no crash, no error) -- see
  // GameTitleRenderHarness.cpp's identical comment/wiring.
  FontDecompressor fontDecompressor;
  if (!fontDecompressor.init()) {
    std::fprintf(stderr, "font decompressor init failed\n");
    return 1;
  }
  FontCacheManager fontCacheManager(renderer.getFontMap(), renderer.getSdCardFonts());
  fontCacheManager.setFontDecompressor(&fontDecompressor);
  renderer.setFontCacheManager(&fontCacheManager);

  GameRenderer gameRenderer;
  gameRenderer.initForTest(kLogicalWidth, kLogicalHeight);
  gameRenderer.drawCorruptSaveNotice(renderer, wholeRun, depth, selection);

  // Dump the framebuffer, un-rotating physical->logical exactly as
  // GameTitleRenderHarness.cpp does (same Portrait rotation convention).
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
  std::fprintf(stderr, "wrote %s (%dx%d), depth=%u selection=%u\n", outPath, kLogicalWidth,
               kLogicalHeight, depth, selection);
  return 0;
}
