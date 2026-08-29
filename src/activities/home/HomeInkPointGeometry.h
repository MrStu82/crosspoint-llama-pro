#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

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

inline std::optional<uint32_t> pagesToMinutes(const uint64_t pages, const uint32_t secondsPerPage) {
  if (secondsPerPage == 0) return std::nullopt;
  if (pages > std::numeric_limits<uint64_t>::max() / secondsPerPage)
    return std::numeric_limits<uint32_t>::max();
  const uint64_t minutes = pages * secondsPerPage / 60U;
  return static_cast<uint32_t>(std::min<uint64_t>(minutes, std::numeric_limits<uint32_t>::max()));
}

// Both estimates use the same per-book measured pace. Whole-book ETA is
// withheld until the existing confidence threshold has been met and progress
// can yield a bounded remaining-page estimate.
inline EtaState estimateEtas(const uint32_t totalSeconds, const uint32_t forwardPages,
                             const bool etaConfident, const int chapterPagesLeft,
                             const bool chapterAvailable, const int progressPercent) {
  const uint32_t secondsPerPage = forwardPages == 0 ? 0 : totalSeconds / forwardPages;
  EtaState result;
  if (chapterAvailable && chapterPagesLeft >= 0)
    result.chapterMinutes = pagesToMinutes(static_cast<uint32_t>(chapterPagesLeft), secondsPerPage);
  if (!etaConfident || progressPercent <= 0 || progressPercent >= 100 || forwardPages == 0)
    return result;

  const uint64_t totalPages = static_cast<uint64_t>(forwardPages) * 100U /
                              static_cast<uint32_t>(progressPercent);
  if (totalPages < forwardPages) return result;
  result.bookMinutes = pagesToMinutes(totalPages - forwardPages, secondsPerPage);
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
