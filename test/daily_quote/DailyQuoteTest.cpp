#include <gtest/gtest.h>

#include <set>
#include <string>

#include "DailyQuote.h"

TEST(DailyQuote, StableWithinLocalDate) {
  const auto& first = DailyQuote::select(2026, 100);
  const auto& again = DailyQuote::select(2026, 100);
  EXPECT_STREQ(first.quote, again.quote);
  EXPECT_STREQ(first.title, again.title);
}

TEST(DailyQuote, LeapYearCycleHas366UniqueAuditedRecords) {
  std::set<std::string> quotes;
  for (int day = 0; day < 366; ++day) {
    const auto& row = DailyQuote::select(2028, day);
    EXPECT_NE(row.quote, nullptr);
    EXPECT_NE(row.character, nullptr);
    EXPECT_NE(row.title, nullptr);
    EXPECT_NE(row.author, nullptr);
    EXPECT_FALSE(std::string(row.quote).empty());
    EXPECT_FALSE(std::string(row.title).empty());
    EXPECT_FALSE(std::string(row.author).empty());
    quotes.insert(row.quote);
  }
  EXPECT_EQ(quotes.size(), DailyQuote::kRecordCount);
}

TEST(DailyQuote, YearRolloverReshuffles) {
  EXPECT_STRNE(DailyQuote::select(2026, 364).quote, DailyQuote::select(2027, 0).quote);
}

// The home screen now derives the day from the RTC's packed YYYYMMDD rather than
// libc localtime, which is only ever set as a side effect of an NTP sync. These
// cases pin the conversion to tm_yday's zero-based convention.
TEST(DailyQuote, DayOfYearMatchesTmYdayConvention) {
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260101), 0);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260201), 31);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20261231), 364);
}

TEST(DailyQuote, DayOfYearHandlesLeapYears) {
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20280229), 59);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20280301), 60);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20281231), 365);
  // 1900 is divisible by 4 but not a leap year; 2000 is.
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(19000301), 59);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20000301), 60);
}

TEST(DailyQuote, DayOfYearRejectsImpossibleDates) {
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(0), -1);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260001), -1);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20261301), -1);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260100), -1);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260229), -1);
  EXPECT_EQ(DailyQuote::dayOfYearFromYmd(20260431), -1);
}

// A rejected date must not be able to reach select() as a valid index.
TEST(DailyQuote, EveryValidDayOfYearIndexesTheDeck) {
  for (int month = 1; month <= 12; ++month) {
    for (int day = 1; day <= 31; ++day) {
      const int index = DailyQuote::dayOfYearFromYmd(20280000 + month * 100 + day);
      if (index < 0) continue;
      EXPECT_LT(index, 366);
      EXPECT_NE(DailyQuote::select(2028, index).quote, nullptr);
    }
  }
}
