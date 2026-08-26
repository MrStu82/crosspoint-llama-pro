#pragma once

#include <cstdint>
#include <string>

struct BookReadingStatsValue {
  uint32_t totalSeconds = 0;
  uint32_t forwardPages = 0;
  bool available = false;

  bool etaConfident() const { return totalSeconds >= 300 && forwardPages >= 5; }
  uint32_t secondsPerPage() const { return forwardPages == 0 ? 0 : totalSeconds / forwardPages; }
};

namespace BookReadingStats {
BookReadingStatsValue read(const std::string& bookPath);
bool add(const std::string& bookPath, uint32_t seconds, uint32_t forwardPages);
}  // namespace BookReadingStats
