#include "game/GameSprites.h"

#include <GfxRenderer.h>

namespace {

// Standard 4x4 Bayer ordered-dither matrix, values 0..15.
constexpr uint8_t kBayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// Per-level plot threshold out of 16 -- how much of the matrix "fires" ink for a given
// grey level. Level 0 (black) always plots; level 3 (white) never does.
constexpr uint8_t kLevelThreshold[4] = {16, 12, 4, 0};

}  // namespace

void drawSprite(GfxRenderer& renderer, int x, int y, const Sprite2bpp& sprite, bool invert) {
  const size_t planeBytes = static_cast<size_t>(sprite.stride) * sprite.h;
  const uint8_t* lsbPlane = sprite.data;
  const uint8_t* msbPlane = sprite.data + planeBytes;

  for (int row = 0; row < sprite.h; ++row) {
    for (int col = 0; col < sprite.w; ++col) {
      const size_t byteIdx = static_cast<size_t>(row) * sprite.stride + (col >> 3);
      const int bitIdx = 7 - (col & 7);  // MSB-first: bit 7 = leftmost pixel
      const int lsbBit = (lsbPlane[byteIdx] >> bitIdx) & 1;
      const int msbBit = (msbPlane[byteIdx] >> bitIdx) & 1;
      int level = lsbBit | (msbBit << 1);
      if (invert) level = 3 - level;

      const uint8_t threshold = kLevelThreshold[level];
      const bool ink = kBayer4x4[row & 3][col & 3] < threshold;
      if (ink) renderer.drawPixel(x + col, y + row, true);
    }
  }
}
