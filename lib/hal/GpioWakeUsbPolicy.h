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

// The production HAL exposes the Phase 2A no-argument stability check. The
// pinned desktop simulator still carries its older two-argument no-op stub.
// Keep that compatibility at this build seam only; production always selects
// the no-argument API and therefore retains the physical debounce semantics.
template <typename Gpio>
inline bool verifyPowerButtonWakeup(Gpio& gpio) {
  if constexpr (requires { gpio.verifyPowerButtonWakeup(); }) {
    return gpio.verifyPowerButtonWakeup();
  } else {
    return gpio.verifyPowerButtonWakeup(0, true);
  }
}

inline constexpr bool shouldUseSplashlessWake(const bool isVerifiedPowerWake, const bool showBootScreen) {
  return isVerifiedPowerWake && !showBootScreen;
}

inline constexpr bool shouldRearmBootScreen(const bool isVerifiedPowerWake, const bool showBootScreen) {
  return !isVerifiedPowerWake && !showBootScreen;
}

// A verified power-button wake may finish after setup has already routed to an
// activity. Consume that wake gesture's release and require a complete release
// before a later hold can put the device back to sleep.
class WakePowerInputGate {
 public:
  void arm(const bool isVerifiedPowerWake) {
    releasePending = isVerifiedPowerWake;
    releasedSinceWake = !isVerifiedPowerWake;
  }

  bool consumeWakeRelease(const bool powerPressed) {
    if (powerPressed) return false;
    releasedSinceWake = true;
    if (!releasePending) return false;
    releasePending = false;
    return true;
  }

  bool allowsPowerActions() const { return releasedSinceWake; }
  bool allowsLongPress(const bool powerPressed) const { return allowsPowerActions() && powerPressed; }

 private:
  bool releasePending = false;
  bool releasedSinceWake = true;
};

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
