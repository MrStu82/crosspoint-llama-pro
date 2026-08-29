#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "BookReadingRate.h"

struct BookReadingStatsValue {
  uint32_t totalSeconds = 0;
  bool available = false;
  bool remainingAvailable = false;
  bool currentRate = false;
  bool fallbackRate = false;
  bool legacyRate = false;
  bool rateConfident = false;
  uint8_t qualifiedSamples = 0;
  uint32_t qualifiedSeconds = 0;
  uint32_t fingerprint = 0;
  uint32_t remainingPagesQ16 = 0;
  uint32_t pagesPerMinuteQ16 = 0;

  bool etaConfident() const { return remainingAvailable && pagesPerMinuteQ16 != 0; }
  std::optional<uint32_t> bookMinutes() const {
    return etaConfident() ? BookReadingRate::etaMinutes(remainingPagesQ16, pagesPerMinuteQ16) : std::nullopt;
  }
};

struct QualifiedPageSample {
  uint16_t dwellSeconds = 0;
  uint32_t fingerprint = 0;
  uint32_t bookHash = 0;
  BookReadingRate::ContentBasis basis = BookReadingRate::ContentBasis::Unknown;
  uint32_t exactRemainingPages = 0;
  uint32_t progressQ24 = 0;
  uint32_t progressDeltaQ24 = 0;
};

namespace BookReadingStats {
BookReadingStatsValue read(const std::string& bookPath);
// Preserves historical TIME READ. Legacy v1 files also migrate a bounded
// forwardPages/totalSeconds pace; new calls record pace only through
// recordQualifiedPage().
bool add(const std::string& bookPath, uint32_t seconds, uint32_t forwardPages);
bool recordQualifiedPage(const std::string& bookPath, const QualifiedPageSample& sample);
bool updatePosition(const std::string& bookPath, uint32_t fingerprint,
                    BookReadingRate::ContentBasis basis, uint32_t exactRemainingPages,
                    uint32_t progressQ24);
}  // namespace BookReadingStats
