#include "DailyQuote.h"

#include <array>
#include <cstdint>

namespace {
constexpr DailyQuoteRecord kQuotes[] = {
#include "DailyQuoteData.inc"
};
static_assert(sizeof(kQuotes) / sizeof(kQuotes[0]) == DailyQuote::kRecordCount);

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

size_t indexFor(int year, int dayOfYear) {
  std::array<uint16_t, DailyQuote::kRecordCount> deck{};
  for (size_t i = 0; i < deck.size(); ++i) deck[i] = static_cast<uint16_t>(i);
  uint32_t random = 0x9e3779b9U ^ static_cast<uint32_t>(year * 2654435761U) ^
                    static_cast<uint32_t>(DailyQuote::kPackVersion * 2246822519U);
  if (random == 0) random = 1;
  for (size_t i = deck.size() - 1; i > 0; --i) {
    const size_t j = nextRandom(random) % (i + 1);
    const auto value = deck[i];
    deck[i] = deck[j];
    deck[j] = value;
  }
  const int boundedDay = dayOfYear < 0 ? 0 : dayOfYear >= static_cast<int>(deck.size())
                                                 ? static_cast<int>(deck.size()) - 1
                                                 : dayOfYear;
  return deck[static_cast<size_t>(boundedDay)];
}
}  // namespace

namespace DailyQuote {
const DailyQuoteRecord& select(int year, int dayOfYear) { return kQuotes[indexFor(year, dayOfYear)]; }

int dayOfYearFromYmd(int yyyymmdd) {
  const int year = yyyymmdd / 10000;
  const int month = (yyyymmdd / 100) % 100;
  const int day = yyyymmdd % 100;
  if (year < 1 || month < 1 || month > 12 || day < 1) return -1;
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  static constexpr int kDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const int monthDays = kDaysInMonth[month - 1] + (leap && month == 2 ? 1 : 0);
  if (day > monthDays) return -1;
  int dayOfYear = day - 1;
  for (int m = 1; m < month; ++m) dayOfYear += kDaysInMonth[m - 1] + (leap && m == 2 ? 1 : 0);
  return dayOfYear;
}
}  // namespace DailyQuote
