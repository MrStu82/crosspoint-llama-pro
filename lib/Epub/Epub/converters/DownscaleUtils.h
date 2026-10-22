#pragma once

#include <stdint.h>

// Resample one 8-bit gray row from srcLen to dstLen pixels. When shrinking,
// each output pixel is the rounded mean of the source pixels it covers, so
// thin lines fade to grey instead of vanishing; when enlarging it picks the
// same source pixel as floor(x * srcLen / dstLen) nearest-neighbour stepping.
inline void resampleRowBox(const uint8_t* src, const int srcLen, uint8_t* dst, const int dstLen) {
  for (int x = 0; x < dstLen; x++) {
    const int s0 = x * srcLen / dstLen;
    const int s1 = (x + 1) * srcLen / dstLen;
    if (s1 <= s0 + 1) {
      dst[x] = src[s0];
      continue;
    }
    uint32_t sum = 0;
    for (int s = s0; s < s1; s++) sum += src[s];
    const uint32_t n = static_cast<uint32_t>(s1 - s0);
    dst[x] = static_cast<uint8_t>((sum + n / 2) / n);
  }
}
