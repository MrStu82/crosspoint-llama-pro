#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>

#include "BookReadingRate.h"

using namespace BookReadingRate;

TEST(BookReadingRate, DwellAcceptsOnlyQualifiedVisibleSingleForwardTurns) {
  DwellTracker tracker;
  tracker.markVisible(1000, 7);
  EXPECT_FALSE(tracker.takeQualifiedForward(5999, 7, true));  // rapid
  tracker.markVisible(1000, 7);
  EXPECT_EQ(tracker.takeQualifiedForward(6000, 7, true), 5);
  tracker.markVisible(1000, 7);
  EXPECT_EQ(tracker.takeQualifiedForward(301000, 7, true), 300);
  tracker.markVisible(1000, 7);
  EXPECT_FALSE(tracker.takeQualifiedForward(301001, 7, true));  // idle
  tracker.markVisible(1000, 7);
  EXPECT_FALSE(tracker.takeQualifiedForward(61000, 7, false));  // jump
}

TEST(BookReadingRate, PauseBackwardAndWrongPageDiscardDwell) {
  DwellTracker tracker;
  tracker.markVisible(1000, 3);
  tracker.pause();
  EXPECT_FALSE(tracker.takeQualifiedForward(61000, 3, true));
  tracker.markVisible(1000, 3);
  EXPECT_FALSE(tracker.takeQualifiedForward(61000, 4, true));
}

TEST(BookReadingRate, MillisWrapIsHandledByUnsignedElapsedArithmetic) {
  DwellTracker tracker;
  tracker.markVisible(std::numeric_limits<uint32_t>::max() - 2999U, 9);
  EXPECT_EQ(tracker.takeQualifiedForward(3000, 9, true), 6);
}

TEST(BookReadingRate, MedianPagesPerMinuteRejectsExtremeQualifiedOutlier) {
  const uint16_t dwell[] = {60, 61, 59, 60, 300};
  const uint32_t rate = pagesPerMinuteQ16(dwell, std::size(dwell));
  EXPECT_NEAR(static_cast<double>(rate) / kQ16One, 1.0, 0.02);
}

TEST(BookReadingRate, ConfidenceAndCurrentBookPrecedenceAreExplicit) {
  EXPECT_FALSE(currentConfident(4, 1000));
  EXPECT_FALSE(currentConfident(5, 299));
  EXPECT_TRUE(currentConfident(5, 300));
  EXPECT_FALSE(overallConfident(20, 1));
  EXPECT_TRUE(overallConfident(20, 2));
  const auto fallback = selectRate(2 * kQ16One, false, kQ16One, true);
  EXPECT_EQ(fallback.source, RateSource::OverallFallback);
  const auto current = selectRate(2 * kQ16One, true, kQ16One, true);
  EXPECT_EQ(current.source, RateSource::CurrentBook);
  EXPECT_EQ(current.pagesPerMinuteQ16, 2U * kQ16One);
}

TEST(BookReadingRate, ExactTxtAndXtcRemainingPagesDoNotUsePercent) {
  EXPECT_EQ(exactRemainingQ16(0, 10), 9U * kQ16One);
  EXPECT_EQ(exactRemainingQ16(8, 10), 1U * kQ16One);
  EXPECT_EQ(exactRemainingQ16(9, 10), 0U);
}

TEST(BookReadingRate, EpubFineContentDeltasBecomeCurrentLayoutPageEquivalents) {
  const uint32_t deltas[] = {kQ24One / 100, kQ24One / 100, kQ24One / 100, kQ24One / 20};
  const uint32_t remaining = remainingFromFineProgressQ16(kQ24One / 4, deltas, std::size(deltas));
  EXPECT_NEAR(static_cast<double>(remaining) / kQ16One, 75.0, 0.1);
}

TEST(BookReadingRate, EtaUsesRemainingPagesDividedByPagesPerMinute) {
  const auto eta = etaMinutes(40U * kQ16One, 2U * kQ16One);
  ASSERT_TRUE(eta);
  EXPECT_EQ(*eta, 20U);
}

TEST(BookReadingRate, LayoutFingerprintChangesWithPaginationInputs) {
  const uint32_t base = layoutFingerprint(1, 480, 800, 3, 16, 1, 20, 0, 0, 0);
  EXPECT_NE(base, layoutFingerprint(1, 480, 800, 4, 16, 1, 20, 0, 0, 0));
  EXPECT_NE(base, layoutFingerprint(1, 480, 800, 3, 18, 1, 20, 0, 0, 0));
  EXPECT_NE(base, layoutFingerprint(1, 800, 480, 3, 16, 1, 20, 0, 1, 0));
}

TEST(BookReadingRate, V1MigrationPreservesTimeButWithholdsRate) {
  LegacyBookV1 legacy;
  legacy.totalSeconds = 1234;
  legacy.forwardPages = 99;
  const StoredBookV2 migrated = migrate(legacy);
  EXPECT_TRUE(valid(migrated));
  EXPECT_EQ(migrated.totalSeconds, 1234U);
  EXPECT_EQ(migrated.sampleCount, 0U);
  EXPECT_EQ(migrated.fingerprint, 0U);
  EXPECT_EQ(migrated.basis, ContentBasis::Unknown);
}

TEST(BookReadingRate, ChecksumsRejectCorruptAndUnknownVersionState) {
  StoredBookV2 book;
  book.totalSeconds = 10;
  seal(book);
  EXPECT_TRUE(valid(book));
  book.totalSeconds ^= 1U;
  EXPECT_FALSE(valid(book));
  seal(book);
  book.version = 99;
  EXPECT_FALSE(valid(book));

  StoredOverallV2 overall;
  seal(overall);
  EXPECT_TRUE(valid(overall));
  overall.samples[0].bookHash = 42;
  EXPECT_FALSE(valid(overall));
}
