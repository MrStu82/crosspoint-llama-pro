#pragma once
#include <cstdint>
#include <cstddef>
namespace safety_guards {
inline constexpr bool shouldHoldPowerLatch(const int8_t pin, const int8_t excludedPowerOffPin,
                                           const bool conflictsWithBus) {
  return pin >= 0 && pin != excludedPowerOffPin && !conflictsWithBus;
}
inline constexpr bool imageChipMatchesDevice(const uint16_t imageChip, const uint16_t deviceChip) {
  return deviceChip == 0xFFFF || imageChip == deviceChip;
}
inline constexpr bool rangeFits(const size_t offset, const size_t length, const size_t total) {
  return offset <= total && length <= total - offset;
}
}  // namespace safety_guards
