#pragma once

#include <stdint.h>

// 4x4 Bayer matrix for ordered dithering
inline const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// Apply Bayer dithering and quantize to 4 levels (0-3)
// Stateless - works correctly with any pixel processing order
inline uint8_t applyBayerDither4Level(uint8_t gray, int x, int y) {
  int bayer = bayer4x4[y & 3][x & 3];
  // Bayer 0..15 centred to -7.5..+7.5 and scaled to +/-40 (symmetric, within half the 85 step).
  int dither = ((bayer * 2 - 15) * 85) / 32;

  int adjusted = gray + dither;
  if (adjusted < 0) adjusted = 0;
  if (adjusted > 255) adjusted = 255;

  // Output levels are 0/85/170/255; cut at their mid-points.
  if (adjusted < 43) return 0;
  if (adjusted < 128) return 1;
  if (adjusted < 213) return 2;
  return 3;
}
