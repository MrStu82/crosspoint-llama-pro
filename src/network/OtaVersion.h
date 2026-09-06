#pragma once

#include <cstdint>
#include <cstring>
#include <limits>

namespace ota_version {
// Parse the three numeric components used by the existing update policy.
// Delivered builds may start with v and carry a git/RC suffix.
inline bool parse(const char* text, uint32_t (&parts)[3]) {
  if (!text) return false;
  if (*text == 'v') ++text;
  for (unsigned i = 0; i < 3; ++i) {
    if (*text < '0' || *text > '9') return false;
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') {
      const uint32_t digit = *text++ - '0';
      if (value > (std::numeric_limits<uint32_t>::max() - digit) / 10) return false;
      value = value * 10 + digit;
    }
    parts[i] = value;
    if (i < 2 && *text++ != '.') return false;
  }
  return *text == '\0' || *text == '-' || *text == '+';
}

inline bool isNewer(const char* current, const char* latest) {
  uint32_t a[3] = {}, b[3] = {};
  if (!parse(current, a) || !parse(latest, b)) return false;
  if (std::strcmp(current, latest) == 0) return false;
  for (unsigned i = 0; i < 3; ++i) {
    if (a[i] != b[i]) return b[i] > a[i];
  }
  // Preserve the existing equal-version RC promotion policy.
  return std::strstr(current, "-rc") != nullptr;
}
}  // namespace ota_version
