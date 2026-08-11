#pragma once

#include "game/Sprite2bpp.h"

class GfxRenderer;

// Shared sprite blit for all games -- see Sprite2bpp.h for the data contract. One
// function, no manager, no atlas, no animation framework: a game owns its own sprite
// arrays and calls this to draw them.
//
// The fast 1-bit refresh path (drawPixel) can't show four true grey levels, so this
// collapses the sprite's 4 levels to 1-bit via an ordered 4x4 Bayer dither -- shape and
// relative shading survive as dither texture. A future quarantined "game mode" LUT
// display path (opt-in per game, true hardware greyscale, cleaned up on exit) can offer
// real 4-level rendering later without this call site changing -- that's separate,
// larger work, not part of this contract.
//
// `invert` flips levels (3-level) for drawing a sprite in "selected tile" contexts
// without needing a second copy of the art.
//
// `scale` plots a scale x scale block of device pixels per source pixel (no new sprite
// bytes needed to render larger). Defaults to 1 so existing call sites are unaffected.
void drawSprite(GfxRenderer& renderer, int x, int y, const Sprite2bpp& sprite, bool invert = false,
                 int scale = 1);
