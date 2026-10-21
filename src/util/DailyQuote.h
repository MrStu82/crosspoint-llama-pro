#pragma once

#include <cstddef>
#include <string>

struct DailyQuoteRecord {
  const char* quote;
  const char* character;
  const char* title;
  const char* author;
};

namespace DailyQuote {
constexpr size_t kRecordCount = 366;
constexpr unsigned kPackVersion = 2;

// dayOfYear is zero-based. Selection is stable for the local day, and the
// yearly permutation contains no repeated record.
const DailyQuoteRecord& select(int year, int dayOfYear);

// Zero-based day of year for a packed YYYYMMDD date, matching tm_yday. Returns
// -1 if the date is not a real calendar date. Kept free of any clock or HAL
// dependency so the caller owns "which day is it" and this stays host-testable.
int dayOfYearFromYmd(int yyyymmdd);

// One comma-separated line, in speaker/title/author order, skipping any field
// the pack leaves empty. Narration has no speaker, so that line collapses to
// "Title, Author" with no leading comma.
std::string attributionLine(const DailyQuoteRecord& record);
}  // namespace DailyQuote
