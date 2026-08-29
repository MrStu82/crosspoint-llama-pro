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

TEST(BookReadingRate, ConfidenceDoesNotSuppressMeasuredRateAndCurrentBookWins) {
  EXPECT_FALSE(currentConfident(4, 1000));
  EXPECT_FALSE(currentConfident(5, 299));
  EXPECT_TRUE(currentConfident(5, 300));
  EXPECT_FALSE(overallConfident(20, 1));
  EXPECT_TRUE(overallConfident(20, 2));
  const auto earlyCurrent = selectRate(2 * kQ16One, false, kQ16One, true);
  EXPECT_EQ(earlyCurrent.source, RateSource::CurrentBook);
  EXPECT_EQ(earlyCurrent.pagesPerMinuteQ16, 2U * kQ16One);
  EXPECT_FALSE(earlyCurrent.confident);
  const auto fallback = selectRate(0, false, kQ16One, false);
  EXPECT_EQ(fallback.source, RateSource::OverallFallback);
  EXPECT_EQ(fallback.pagesPerMinuteQ16, kQ16One);
  EXPECT_FALSE(fallback.confident);
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

TEST(BookReadingRate, V1MigrationPreservesTimeAndUsableForwardPageRate) {
  LegacyBookV1 legacy;
  legacy.totalSeconds = 1234;
  legacy.forwardPages = 99;
  const StoredBookV3 migrated = migrate(legacy);
  EXPECT_TRUE(valid(migrated));
  EXPECT_EQ(migrated.totalSeconds, 1234U);
  EXPECT_EQ(migrated.legacyPagesPerMinuteQ16,
            static_cast<uint32_t>(99ULL * 60U * kQ16One / 1234U));
  EXPECT_EQ(migrated.sampleCount, 0U);
  EXPECT_EQ(migrated.fingerprint, 0U);
  EXPECT_EQ(migrated.basis, ContentBasis::Unknown);
}

TEST(BookReadingRate, V1MigrationRejectsMathematicallyUnusablePace) {
  LegacyBookV1 rapid;
  rapid.totalSeconds = 1;
  rapid.forwardPages = 100;
  EXPECT_EQ(migrate(rapid).legacyPagesPerMinuteQ16, 0U);
  LegacyBookV1 idle;
  idle.totalSeconds = 601;
  idle.forwardPages = 2;
  EXPECT_EQ(migrate(idle).legacyPagesPerMinuteQ16, 0U);
}

TEST(BookReadingRate, DeployedV2MigrationPreservesQualifiedSamplesPositionAndTime) {
  StoredBookV2 old;
  old.totalSeconds = 900;
  old.fingerprint = 77;
  old.basis = ContentBasis::ExactPages;
  old.remainingPagesQ16 = 42U * kQ16One;
  old.sampleCount = 1;
  old.sampleNext = 1;
  old.samples[0] = RateSample{30, 0, 0};
  seal(old);
  const StoredBookV3 migrated = migrate(old);
  EXPECT_TRUE(valid(migrated));
  EXPECT_EQ(migrated.totalSeconds, 900U);
  EXPECT_EQ(migrated.fingerprint, 77U);
  EXPECT_EQ(migrated.remainingPagesQ16, 42U * kQ16One);
  EXPECT_EQ(migrated.sampleCount, 1U);
  EXPECT_EQ(migrated.samples[0].dwellSeconds, 30U);
  EXPECT_EQ(migrated.legacyPagesPerMinuteQ16, 0U);
}

TEST(BookReadingRate, ChecksumsRejectCorruptAndUnknownVersionState) {
  StoredBookV3 book;
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
