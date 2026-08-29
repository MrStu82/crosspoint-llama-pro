#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>

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

TEST(HomeInkPointGeometry, PartialRatingUsesAClippedLeftHalfAndCompleteOutline) {
  const auto& halfX = InkPointHomeGeometry::kHalfRatingStarX;
  const auto& halfY = InkPointHomeGeometry::kHalfRatingStarY;
  ASSERT_EQ(halfX.size(), 6U);
  ASSERT_EQ(halfX.size(), halfY.size());
  EXPECT_EQ(halfX.front(), 0);
  EXPECT_EQ(halfY.front(), -10);
  EXPECT_EQ(halfX[1], 0);
  EXPECT_EQ(halfY[1], 5);
  for (const int x : halfX) EXPECT_LE(x, 0);
  EXPECT_EQ(InkPointHomeGeometry::ratingStarFill(444, 3),
            InkPointHomeGeometry::RatingStarFill::Full);
  EXPECT_EQ(InkPointHomeGeometry::ratingStarFill(444, 4),
            InkPointHomeGeometry::RatingStarFill::Half);
  EXPECT_EQ(InkPointHomeGeometry::ratingStarFill(400, 4),
            InkPointHomeGeometry::RatingStarFill::Outline);
}

TEST(HomeInkPointGeometry, ProgressBarBeginsDirectlyBelowUnframedCover) {
  const auto layout = InkPointHomeGeometry::coverProgressLayout(20, 116, 220, 434, 61);
  EXPECT_EQ(layout.x, 20);
  EXPECT_EQ(layout.y, 550);
  EXPECT_EQ(layout.width, 220);
  EXPECT_EQ(layout.height, 14);
  EXPECT_EQ(layout.fillX, 21);
  EXPECT_EQ(layout.fillY, 554);
  EXPECT_EQ(layout.fillWidth, 132);
  EXPECT_EQ(layout.fillHeight, 6);
}

TEST(HomeInkPointGeometry, WholeBookEtaOwnsTheRowBelowChapter) {
  const auto layout = InkPointHomeGeometry::statsLayout(260, 305, 58);
  EXPECT_EQ(layout.timeX, 260);
  EXPECT_EQ(layout.timeY, 305);
  EXPECT_EQ(layout.chapterX, 260);
  EXPECT_EQ(layout.chapterY, 363);
  EXPECT_EQ(layout.bookX, 260);
  EXPECT_EQ(layout.bookY, 421);
  EXPECT_GE(layout.bookY, layout.chapterY + 58);
  EXPECT_EQ(layout.chevronY, 507);
  EXPECT_GT(layout.chevronY, layout.bookY + 20);
  EXPECT_LT(layout.chevronY, 550);  // Existing Stats tap target bottom.
}

TEST(HomeInkPointGeometry, EtaUsesOnlyTheMeasuredPerBookRate) {
  // 600s / 10 pages = 60s/page. At 20% the estimated total is 50 pages,
  // leaving 40; the current chapter has 3 pages left.
  const auto eta = InkPointHomeGeometry::estimateEtas(600, 10, true, 3, true, 20);
  ASSERT_TRUE(eta.chapterMinutes);
  ASSERT_TRUE(eta.bookMinutes);
  EXPECT_EQ(*eta.chapterMinutes, 3U);
  EXPECT_EQ(*eta.bookMinutes, 40U);
}

TEST(HomeInkPointGeometry, WholeBookEtaIsHiddenWhenEvidenceIsInsufficient) {
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(299, 5, false, 3, true, 20).bookMinutes);
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(600, 10, true, 3, true, 0).bookMinutes);
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(600, 10, true, 3, true, 100).bookMinutes);
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(600, 0, false, 3, true, 20).bookMinutes);
}

TEST(HomeInkPointGeometry, EtaArithmeticIsBoundedForCorruptExtremeState) {
  const auto eta = InkPointHomeGeometry::estimateEtas(
      std::numeric_limits<uint32_t>::max(), 5, true,
      std::numeric_limits<int>::max(), true, 1);
  ASSERT_TRUE(eta.chapterMinutes);
  ASSERT_TRUE(eta.bookMinutes);
  EXPECT_EQ(*eta.chapterMinutes, std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(*eta.bookMinutes, std::numeric_limits<uint32_t>::max());
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
