#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <utility>

#include "LineGeometry.h"

// thickLineOffsetInX is the exact function GfxRenderer::drawLine's
// multi-width overload calls -- these tests exercise production logic
// directly, not a reimplementation, so there is no drift risk.

TEST(ThickLineOffsetInX, HorizontalLineWidensInY) {
  // y1 == y2: dy == 0, dx > 0 -- must NOT offset in x.
  EXPECT_FALSE(thickLineOffsetInX(/*dx=*/100, /*dy=*/0));
}

TEST(ThickLineOffsetInX, VerticalLineWidensInX) {
  // x1 == x2: dx == 0, dy > 0 -- must offset in x (the bug this fixes:
  // the old code always offset in y here, stretching the line instead).
  EXPECT_TRUE(thickLineOffsetInX(/*dx=*/0, /*dy=*/100));
}

TEST(ThickLineOffsetInX, DiagonalFollowsDominantAxis) {
  EXPECT_TRUE(thickLineOffsetInX(/*dx=*/3, /*dy=*/10));   // steeper than 45 deg
  EXPECT_FALSE(thickLineOffsetInX(/*dx=*/10, /*dy=*/3));  // shallower than 45 deg
}

// Simulates GfxRenderer::drawLine(x1,y1,x2,y2,lineWidth,state)'s exact
// per-iteration offset math (mirrors GfxRenderer.cpp's loop verbatim) to
// prove the *shape* a thickened line covers, not just the axis decision in
// isolation.
static std::set<std::pair<int, int>> simulateThickSegmentEndpoints(int x1, int y1, int x2, int y2, int lineWidth) {
  std::set<std::pair<int, int>> endpoints;
  const int half = lineWidth / 2;
  const int dx = std::abs(x2 - x1);
  const int dy = std::abs(y2 - y1);
  const bool offsetInX = thickLineOffsetInX(dx, dy);
  for (int i = 0; i < lineWidth; i++) {
    const int off = i - half;
    if (offsetInX) {
      endpoints.insert({x1 + off, y1});
      endpoints.insert({x2 + off, y2});
    } else {
      endpoints.insert({x1, y1 + off});
      endpoints.insert({x2, y2 + off});
    }
  }
  return endpoints;
}

TEST(ThickLineShape, VerticalLineWidensAsBandNotStretch) {
  // A vertical line from (10,0) to (10,50), width 5. Correct: a 5px-wide
  // band spanning x in [8,12], with every sub-line still running y 0->50
  // (i.e. the endpoints' y coordinates never move). The pre-fix bug would
  // have offset y instead, stretching the segment's length by lineWidth.
  const auto endpoints = simulateThickSegmentEndpoints(10, 0, 10, 50, 5);

  int minX = 1 << 30, maxX = -(1 << 30);
  for (const auto& [ex, ey] : endpoints) {
    minX = std::min(minX, ex);
    maxX = std::max(maxX, ex);
    // Every offset endpoint must still land on y=0 or y=50 -- the length
    // (y-span) must be untouched by thickening.
    EXPECT_TRUE(ey == 0 || ey == 50) << "y=" << ey << " drifted off the line's original endpoints";
  }
  EXPECT_EQ(minX, 8);
  EXPECT_EQ(maxX, 12);
}

TEST(ThickLineShape, HorizontalLineWidensAsBandNotStretch) {
  const auto endpoints = simulateThickSegmentEndpoints(0, 20, 60, 20, 7);

  int minY = 1 << 30, maxY = -(1 << 30);
  for (const auto& [ex, ey] : endpoints) {
    minY = std::min(minY, ey);
    maxY = std::max(maxY, ey);
    EXPECT_TRUE(ex == 0 || ex == 60) << "x=" << ex << " drifted off the line's original endpoints";
  }
  EXPECT_EQ(minY, 17);
  EXPECT_EQ(maxY, 23);
}
