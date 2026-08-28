#pragma once

#include <array>
#include <algorithm>

namespace InkPointHomeGeometry {

// Alternating outer/inner vertices, clockwise from the top point.  This is a
// real five-point polygon used by the raster renderer, never a font glyph.
inline constexpr std::array<int, 10> kRatingStarX = {0, 3, 10, 5, 6, 0, -6, -5, -10, -3};
inline constexpr std::array<int, 10> kRatingStarY = {-10, -3, -3, 2, 9, 5, 9, 2, -3, -3};

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
