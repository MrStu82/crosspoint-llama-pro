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
  const auto eta = InkPointHomeGeometry::estimateEtas(
      BookReadingRate::kQ16One, 40U * BookReadingRate::kQ16One, true, 3, true);
  ASSERT_TRUE(eta.chapterMinutes);
  ASSERT_TRUE(eta.bookMinutes);
  EXPECT_EQ(*eta.chapterMinutes, 3U);
  EXPECT_EQ(*eta.bookMinutes, 40U);
}

TEST(HomeInkPointGeometry, OneUsableMeasuredSampleProducesBothHomeEtaValues) {
  const uint16_t oneRealDwell[] = {30};
  const uint32_t measuredRate = BookReadingRate::pagesPerMinuteQ16(oneRealDwell, 1);
  const auto selected = BookReadingRate::selectRate(
      measuredRate, false, BookReadingRate::kQ16One, true);
  ASSERT_EQ(selected.source, BookReadingRate::RateSource::CurrentBook);
  EXPECT_FALSE(selected.confident);
  const auto eta = InkPointHomeGeometry::estimateEtas(
      selected.pagesPerMinuteQ16, 40U * BookReadingRate::kQ16One, true, 6, true);
  ASSERT_TRUE(eta.chapterMinutes);
  ASSERT_TRUE(eta.bookMinutes);
  EXPECT_EQ(*eta.chapterMinutes, 3U);
  EXPECT_EQ(*eta.bookMinutes, 20U);
}

TEST(HomeInkPointGeometry, OverallMeasuredRateIsUsedOnlyWhenCurrentBookHasNone) {
  const auto selected = BookReadingRate::selectRate(
      0, false, BookReadingRate::kQ16One, false);
  ASSERT_EQ(selected.source, BookReadingRate::RateSource::OverallFallback);
  const auto eta = InkPointHomeGeometry::estimateEtas(
      selected.pagesPerMinuteQ16, 20U * BookReadingRate::kQ16One, true, 4, true);
  ASSERT_TRUE(eta.chapterMinutes);
  ASSERT_TRUE(eta.bookMinutes);
  EXPECT_EQ(*eta.chapterMinutes, 4U);
  EXPECT_EQ(*eta.bookMinutes, 20U);
}

TEST(HomeInkPointGeometry, WholeBookEtaIsHiddenWhenEvidenceIsInsufficient) {
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(BookReadingRate::kQ16One, 40U << 16, false, 3, true)
                   .bookMinutes);
  EXPECT_FALSE(InkPointHomeGeometry::estimateEtas(0, 40U << 16, true, 3, true).bookMinutes);
}

TEST(HomeInkPointGeometry, EtaArithmeticIsBoundedForCorruptExtremeState) {
  const auto eta = InkPointHomeGeometry::estimateEtas(
      1, std::numeric_limits<uint32_t>::max(), true,
      std::numeric_limits<int>::max(), true);
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
