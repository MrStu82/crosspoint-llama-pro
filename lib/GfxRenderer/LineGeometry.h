#pragma once

// Pure geometry helper for GfxRenderer::drawLine's multi-width overload.
// Extracted so it can be exercised by a host-side unit test with zero HAL
// dependencies -- the production code and the test both call this exact
// function, so there is no risk of the test drifting from what actually
// ships.
//
// Decides which axis a thick line should widen along: a line whose height
// span (dy) exceeds its width span (dx) is "more vertical" and must widen
// in x (or thickening just stretches its length); otherwise it widens in y.
inline bool thickLineOffsetInX(int dx, int dy) {
  return dy > dx;
}
