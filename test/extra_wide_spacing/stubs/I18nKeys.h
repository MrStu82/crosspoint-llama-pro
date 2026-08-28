#pragma once

#include <cstddef>
#include <cstdint>

enum class Language : uint8_t { EN = 0 };
inline constexpr const char* LANGUAGE_CODES[] = {"EN"};
inline constexpr size_t getLanguageCount() { return 1; }

enum class StrId : uint16_t {
  STR_TIGHT,
  STR_NORMAL,
  STR_WIDE,
  STR_EXTRA_WIDE,
};
