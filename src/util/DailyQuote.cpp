#include "DailyQuote.h"

#include <array>
#include <cstdint>
#include <ctime>

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

const DailyQuoteRecord& localToday() {
  const std::time_t now = std::time(nullptr);
  const std::tm* local = now > 0 ? std::localtime(&now) : nullptr;
  return select(local ? local->tm_year + 1900 : 2026, local ? local->tm_yday : 0);
}
}  // namespace DailyQuote
