#include "WifiAwakeLock.h"

#include <WiFi.h>

namespace {
bool readWifiSleep() {
#ifdef SIMULATOR
  // The simulator's WiFi facade has no readable modem-sleep state. Its
  // setSleep() is inert, so restoring the hardware default is sufficient.
  return true;
#else
  return static_cast<int>(WiFi.getSleep()) != 0;
#endif
}

void writeWifiSleep(const bool enabled) { WiFi.setSleep(enabled); }
}  // namespace

WifiAwakeLock::WifiAwakeLock() : WifiAwakeLock(readWifiSleep, writeWifiSleep) {}

WifiAwakeLock::WifiAwakeLock(const GetSleep getSleep, const SetSleep setSleep)
    : getSleep(getSleep), setSleep(setSleep) {}

WifiAwakeLock::~WifiAwakeLock() { release(); }

void WifiAwakeLock::acquire() {
  if (held) return;
  restoreSleep = getSleep();
  setSleep(false);
  held = true;
}

void WifiAwakeLock::release() {
  if (!held) return;
  setSleep(restoreSleep);
  held = false;
}
