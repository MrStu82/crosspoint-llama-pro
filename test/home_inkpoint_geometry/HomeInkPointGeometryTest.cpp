#include <gtest/gtest.h>

#include <cstdlib>

#include "HomeInkPointGeometry.h"

TEST(HomeInkPointGeometry, FilledRatingUsesFivePointAlternatingPolygon) {
  const auto& x = InkPointHomeGeometry::kRatingStarX;
  const auto& y = InkPointHomeGeometry::kRatingStarY;
  ASSERT_EQ(x.size(), 10U);
  ASSERT_EQ(y.size(), 10U);
  for (size_t i = 0; i < x.size(); i += 2) {
    const int outerRadiusSquared = x[i] * x[i] + y[i] * y[i];
    const int previousInner = (i + x.size() - 1) % x.size();
    const int nextInner = (i + 1) % x.size();
    EXPECT_GT(outerRadiusSquared, x[previousInner] * x[previousInner] + y[previousInner] * y[previousInner]);
    EXPECT_GT(outerRadiusSquared, x[nextInner] * x[nextInner] + y[nextInner] * y[nextInner]);
  }
}

TEST(HomeInkPointGeometry, QuoteAndOneLineAttributionAreCenteredInActualFreeBand) {
  constexpr int kProgressBottom = 546;
  constexpr int kFooterTop = 728;
  const auto layout = InkPointHomeGeometry::centerQuoteBlock(
      kProgressBottom, kFooterTop, 3, 25, 16, 18, 20, 12);
  EXPECT_EQ(layout.height, 109);
  EXPECT_LE(std::abs((layout.top - kProgressBottom) -
                     (kFooterTop - (layout.top + layout.height))), 1);
  EXPECT_EQ(layout.quoteBaseline, layout.top + 20);
  EXPECT_EQ(layout.attributionBaseline, layout.top + 3 * 25 + 16 + 12);
}

TEST(HomeInkPointGeometry, OverlongBlockStaysAnchoredToFreeBandTop) {
  const auto layout = InkPointHomeGeometry::centerQuoteBlock(546, 728, 8, 25, 16, 18, 20, 12);
  EXPECT_EQ(layout.top, 546);
}
