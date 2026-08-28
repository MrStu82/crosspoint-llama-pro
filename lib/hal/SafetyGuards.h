#pragma once
#include <cstdint>
namespace safety_guards {
inline constexpr bool shouldHoldPowerLatch(const int8_t pin, const int8_t excludedPowerOffPin,
                                           const bool conflictsWithBus) {
  return pin >= 0 && pin != excludedPowerOffPin && !conflictsWithBus;
}
inline constexpr bool imageChipMatchesDevice(const uint16_t imageChip, const uint16_t deviceChip) {
  return deviceChip == 0xFFFF || imageChip == deviceChip;
}
}  // namespace safety_guards
