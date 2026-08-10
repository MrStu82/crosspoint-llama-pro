#pragma once

#include <cstdint>

// Shared 2bpp sprite contract for all games (Tamagotchi, Tetris, Solitaire, Minesweeper,
// Sudoku, Deep Mines) -- one struct, one blit, matches build-scripts/sprite_conv.mjs 1:1
// so a converter-generated header slots in as a straight array replacement with zero
// code change on the firmware side.
//
// Levels: 0 = black, 1 = dark grey, 2 = light grey, 3 = white; value = lsb | (msb << 1).
// Row-major, MSB-first bit order within each byte (bit 7 = leftmost pixel).
// `data` is one flat array: the entire LSB plane (stride*h bytes), then the entire MSB
// plane (stride*h bytes).
struct Sprite2bpp {
  uint16_t w;
  uint16_t h;
  uint16_t stride;  // ceil(w/8)
  const uint8_t* data;
};
