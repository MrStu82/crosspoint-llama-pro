#pragma once

#include <cstdint>

namespace gpio_policy {

enum class UsbDetectionSource : uint8_t { X3GaugeCurrent, DigitalPin, ChargingState, Unsupported };

inline constexpr bool isPhysicalButtonPressed(const bool pinConfigured, const bool activeHigh,
                                               const bool sampledHigh) {
  return pinConfigured && (sampledHigh == activeHigh);
}

inline constexpr bool isStablePowerWake(const bool heldAtFirstSample, const bool heldAfterDebounce) {
  return heldAtFirstSample && heldAfterDebounce;
}

inline constexpr bool shouldUsePersistedSleepFrame(const bool isVerifiedPowerWake, const bool showBootScreen) {
  return isVerifiedPowerWake && !showBootScreen;
}

inline constexpr UsbDetectionSource selectUsbDetectionSource(const bool isX3, const bool hasDigitalDetect,
                                                              const bool supportsChargingFallback) {
  if (isX3) return UsbDetectionSource::X3GaugeCurrent;
  if (hasDigitalDetect) return UsbDetectionSource::DigitalPin;
  if (supportsChargingFallback) return UsbDetectionSource::ChargingState;
  return UsbDetectionSource::Unsupported;
}

inline constexpr bool usbConnectedFromCurrent(const bool readingKnown, const int16_t currentMa) {
  return readingKnown && currentMa > 0;
}

inline constexpr bool usbConnectedFromCharging(const bool chargingKnown, const bool charging) {
  return chargingKnown && charging;
}

}  // namespace gpio_policy
