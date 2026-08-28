#pragma once

#include <cstddef>
#include <cstdint>

inline constexpr uint8_t NOTO_READER_POINT_SIZES[] = {12, 14, 16, 18};
inline constexpr uint8_t CROSSINK_READER_POINT_SIZES[] = {10, 12, 14, 16};
inline constexpr uint8_t BUILTIN_READER_POINT_SIZES[] = {10, 12, 14, 16};

inline uint8_t snapToNearestPointSize(const uint8_t* sizes, size_t count, uint8_t point) {
  uint8_t best = sizes[0];
  for (size_t i = 1; i < count; ++i) {
    const int currentDistance = sizes[i] > point ? sizes[i] - point : point - sizes[i];
    const int bestDistance = best > point ? best - point : point - best;
    if (currentDistance < bestDistance) best = sizes[i];
  }
  return best;
}
