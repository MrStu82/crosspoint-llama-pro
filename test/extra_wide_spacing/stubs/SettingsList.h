#pragma once

#include "CrossPointSettings.h"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class SettingType { ENUM, TOGGLE, VALUE };

struct SettingInfo {
  struct ValueRange {
    uint8_t min = 0;
    uint8_t max = 0;
  };

  const char* key = nullptr;
  uint8_t CrossPointSettings::*valuePtr = nullptr;
  size_t stringOffset = 0;
  bool obfuscated = false;
  size_t stringMaxLen = 0;
  SettingType type = SettingType::ENUM;
  std::vector<uint8_t> enumValues;
  ValueRange valueRange;
};

inline std::vector<SettingInfo> getSettingsList() {
  SettingInfo lineSpacing;
  lineSpacing.key = "lineSpacing";
  lineSpacing.valuePtr = &CrossPointSettings::lineSpacing;
  lineSpacing.type = SettingType::ENUM;
  lineSpacing.enumValues = {0, 1, 2, 3};
  return {lineSpacing};
}
