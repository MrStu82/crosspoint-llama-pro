#include <cstdlib>
#include <iostream>
#include <vector>

#include <WiFi.h>

#include "WifiAwakeLock.h"

namespace {
bool sleepEnabled = true;
std::vector<bool> writes;
int checks = 0;

bool readSleep() { return sleepEnabled; }

void writeSleep(const bool enabled) {
  sleepEnabled = enabled;
  writes.push_back(enabled);
}

void reset(const bool initialSleep = true) {
  sleepEnabled = initialSleep;
  writes.clear();
}

void check(const bool condition, const char* name) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
    std::exit(1);
  }
}
}  // namespace

WiFiClass WiFi;

bool WiFiClass::getSleep() const { return sleepEnabled; }

bool WiFiClass::setSleep(const bool enabled) {
  writeSleep(enabled);
  return true;
}

int main() {
  // Successful network lifecycle: acquire before HTTP, release afterward.
  reset();
  {
    WifiAwakeLock lock(readSleep, writeSleep);
    lock.acquire();
    check(lock.isHeld(), "success acquires lock");
    check(!sleepEnabled, "success disables modem sleep");
    lock.release();
    check(!lock.isHeld(), "success releases lock");
    check(sleepEnabled, "success restores modem sleep");
  }
  check(writes == std::vector<bool>({false, true}), "success writes one acquire and release");

  // Failed HTTP follows the same release path and leaves no lock behind.
  reset();
  {
    WifiAwakeLock lock(readSleep, writeSleep);
    lock.acquire();
    lock.release();
    check(!lock.isHeld(), "failure releases lock");
    check(sleepEnabled, "failure restores modem sleep");
  }
  check(writes == std::vector<bool>({false, true}), "failure writes one acquire and release");

  // Cancelled WiFi selection never acquired the lock; release is harmless.
  reset();
  {
    WifiAwakeLock lock(readSleep, writeSleep);
    lock.release();
    check(!lock.isHeld(), "cancellation remains unlocked");
    check(writes.empty(), "cancellation does not touch WiFi sleep");
  }

  // Activity-exit/destructor is the structural backstop for an active operation.
  reset();
  {
    WifiAwakeLock lock(readSleep, writeSleep);
    lock.acquire();
    check(lock.isHeld(), "activity operation acquires lock");
  }
  check(sleepEnabled, "activity exit restores modem sleep");
  check(writes == std::vector<bool>({false, true}), "activity exit releases exactly once");

  // Preserve an existing owner that had already disabled modem sleep.
  reset(false);
  {
    WifiAwakeLock lock(readSleep, writeSleep);
    lock.acquire();
    lock.acquire();
    check(writes == std::vector<bool>({false}), "acquisition is idempotent");
    lock.release();
    lock.release();
  }
  check(!sleepEnabled, "release restores prior disabled state");
  check(writes == std::vector<bool>({false, false}), "release is idempotent");

  // Production constructor delegates through the WiFi facade without changing
  // the lifecycle semantics tested above.
  reset();
  {
    WifiAwakeLock lock;
    lock.acquire();
    check(lock.isHeld() && !sleepEnabled, "production adapter acquires");
    lock.release();
  }
  check(sleepEnabled, "production adapter releases");
  check(writes == std::vector<bool>({false, true}), "production adapter writes expected states");

  std::cout << "PASS: " << checks << " deterministic KOReader WiFi-awake checks\n";
  return 0;
}
