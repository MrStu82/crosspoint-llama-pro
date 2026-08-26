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
