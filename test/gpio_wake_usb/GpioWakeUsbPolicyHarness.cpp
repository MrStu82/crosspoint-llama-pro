#include <cstdlib>
#include <iostream>

#include "GpioWakeUsbPolicy.h"

namespace {
int checks = 0;

void check(bool condition, const char* name) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}
}  // namespace

int main() {
  using gpio_policy::UsbDetectionSource;

  check(gpio_policy::isStablePowerWake(true, true), "wake held across debounce");
  check(!gpio_policy::isStablePowerWake(true, false), "wake released during debounce");
  check(!gpio_policy::isStablePowerWake(false, true), "wake asserted after first sample");
  check(!gpio_policy::isStablePowerWake(false, false), "wake never asserted");
  check(gpio_policy::shouldUsePersistedSleepFrame(true, false), "verified wake may use retained frame");
  check(!gpio_policy::shouldUsePersistedSleepFrame(false, false), "cold boot rejects stale retained frame");
  check(!gpio_policy::shouldUsePersistedSleepFrame(true, true), "boot-screen request wins on wake");

  check(gpio_policy::isPhysicalButtonPressed(true, false, false), "active-low held");
  check(!gpio_policy::isPhysicalButtonPressed(true, false, true), "active-low released");
  check(gpio_policy::isPhysicalButtonPressed(true, true, true), "active-high held");
  check(!gpio_policy::isPhysicalButtonPressed(true, true, false), "active-high released");
  check(!gpio_policy::isPhysicalButtonPressed(false, false, false), "unconfigured power pin");

  check(gpio_policy::selectUsbDetectionSource(true, true, true) == UsbDetectionSource::X3GaugeCurrent,
        "X3 gauge precedence");
  check(gpio_policy::usbConnectedFromCurrent(true, 1), "positive X3 current");
  check(!gpio_policy::usbConnectedFromCurrent(true, 0), "zero X3 current");
  check(!gpio_policy::usbConnectedFromCurrent(true, -1), "negative X3 current");
  check(!gpio_policy::usbConnectedFromCurrent(false, 100), "failed X3 current read");

  check(gpio_policy::selectUsbDetectionSource(false, true, true) == UsbDetectionSource::DigitalPin,
        "digital detect precedence");
  check(gpio_policy::selectUsbDetectionSource(false, false, true) == UsbDetectionSource::ChargingState,
        "supported charging fallback");
  check(gpio_policy::usbConnectedFromCharging(true, true), "known charging");
  check(!gpio_policy::usbConnectedFromCharging(true, false), "known not charging");
  check(!gpio_policy::usbConnectedFromCharging(false, true), "unknown charging state");
  check(gpio_policy::selectUsbDetectionSource(false, false, false) == UsbDetectionSource::Unsupported,
        "unsupported board rejects charging inference");

  std::cout << "PASS: " << checks << " deterministic GPIO policy checks\n";
  return 0;
}
