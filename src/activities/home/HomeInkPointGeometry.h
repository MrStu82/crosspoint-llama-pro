#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

#include "util/BookReadingRate.h"

namespace InkPointHomeGeometry {

// Alternating outer/inner vertices, clockwise from the top point.  This is a
// real five-point polygon used by the raster renderer, never a font glyph.
inline constexpr std::array<int, 10> kRatingStarX = {0, 3, 10, 5, 6, 0, -6, -5, -10, -3};
inline constexpr std::array<int, 10> kRatingStarY = {-10, -3, -3, 2, 9, 5, 9, 2, -3, -3};

// The left half is the exact intersection of the rating-star polygon with
// x <= 0. Filling this polygon before redrawing the complete outline leaves a
// crisp outlined right half instead of the old horizontal hatch approximation.
inline constexpr std::array<int, 6> kHalfRatingStarX = {0, 0, -6, -5, -10, -3};
inline constexpr std::array<int, 6> kHalfRatingStarY = {-10, 5, 9, 2, -3, -3};

enum class RatingStarFill : uint8_t { Outline, Half, Full };

inline RatingStarFill ratingStarFill(const int valueX100, const int starIndex) {
  if (starIndex < 0 || starIndex >= 5 || valueX100 <= starIndex * 100)
    return RatingStarFill::Outline;
  if (valueX100 >= (starIndex + 1) * 100) return RatingStarFill::Full;
  return RatingStarFill::Half;
}

inline constexpr int kCoverProgressGap = 0;
inline constexpr int kCoverProgressHeight = 14;
inline constexpr int kCoverProgressFillHeight = 6;
inline constexpr int kQuoteBandTopOffsetFromCoverBottom = 8;
inline constexpr int kStatsToChevronGap = 8;

struct CoverProgressLayout {
  int x;
  int y;
  int width;
  int height;
  int fillX;
  int fillY;
  int fillWidth;
  int fillHeight;
};

inline CoverProgressLayout coverProgressLayout(const int coverX, const int coverY,
                                               const int coverWidth, const int coverHeight,
                                               const int progressPercent) {
  const int innerWidth = std::max(0, coverWidth - 2);
  const int boundedPercent = std::clamp(progressPercent, 0, 100);
  return {coverX,
          coverY + coverHeight + kCoverProgressGap,
          coverWidth,
          kCoverProgressHeight,
          coverX + 1,
          coverY + coverHeight + kCoverProgressGap +
              (kCoverProgressHeight - kCoverProgressFillHeight) / 2,
          innerWidth * boundedPercent / 100,
          kCoverProgressFillHeight};
}

struct StatsLayout {
  int timeX;
  int timeY;
  int chapterX;
  int chapterY;
  int bookX;
  int bookY;
  int chevronY;
};

// Each ETA owns a full row. On the physical X4 Pro, BOOK LEFT beside CHAPTER
// LEFT collided once translated text and real values were rasterised.
inline StatsLayout statsLayout(const int rightX, const int statStart, const int statStep) {
  constexpr int kValueOffset = 20;
  return {rightX, statStart, rightX, statStart + statStep,
          rightX, statStart + 2 * statStep,
          statStart + 3 * statStep + kValueOffset + kStatsToChevronGap};
}

struct EtaState {
  std::optional<uint32_t> chapterMinutes;
  std::optional<uint32_t> bookMinutes;
};

// Both estimates use the selected robust pages/minute rate. Whole-book ETA is
// withheld unless the reader persisted compatible remaining page-equivalents;
// rounded 0-100 progress is deliberately not part of this contract.
inline EtaState estimateEtas(const uint32_t pagesPerMinuteQ16, const uint32_t remainingPagesQ16,
                             const bool remainingAvailable, const int chapterPagesLeft,
                             const bool chapterAvailable) {
  EtaState result;
  if (chapterAvailable && chapterPagesLeft >= 0 && pagesPerMinuteQ16 != 0) {
    const uint64_t q16 = static_cast<uint64_t>(static_cast<uint32_t>(chapterPagesLeft)) << 16;
    result.chapterMinutes = BookReadingRate::etaMinutes(
        static_cast<uint32_t>(std::min<uint64_t>(q16, std::numeric_limits<uint32_t>::max())), pagesPerMinuteQ16);
  }
  if (remainingAvailable)
    result.bookMinutes = BookReadingRate::etaMinutes(remainingPagesQ16, pagesPerMinuteQ16);
  return result;
}

struct QuoteBlockLayout {
  int top;
  int height;
  int quoteBaseline;
  int attributionBaseline;
};

inline QuoteBlockLayout centerQuoteBlock(const int bandTop, const int bandBottom,
                                         const int quoteLineCount, const int quoteLineHeight,
                                         const int quoteToAttributionGap,
                                         const int attributionLineHeight,
                                         const int quoteAscender, const int attributionAscender) {
  const int safeQuoteLines = std::max(1, quoteLineCount);
  const int height = safeQuoteLines * quoteLineHeight + quoteToAttributionGap + attributionLineHeight;
  const int top = bandTop + std::max(0, (bandBottom - bandTop - height) / 2);
  return {top, height, top + quoteAscender,
          top + safeQuoteLines * quoteLineHeight + quoteToAttributionGap + attributionAscender};
}

}  // namespace InkPointHomeGeometry
