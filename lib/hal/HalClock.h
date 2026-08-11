#pragma once

#include <Arduino.h>
#include <Rtc.h>

class HalClock;
extern HalClock halClock;  // Singleton

class HalClock {
  bool _available = false;
  mutable Rtc _sdkRtc;
  mutable Rtc::DateTime _cachedDt{};
  mutable bool _hasCachedTime = false;
  mutable unsigned long _lastPollMs = 0;
  mutable unsigned long _lastGoodMs = 0;  // millis() of the last real (non-cache) successful I2C read

  static constexpr unsigned long CLOCK_POLL_MS = 10000;  // 10 seconds
  // Ceiling on how long a stale cached reading may be served after I2C reads start
  // failing (e.g. a bus glitch/dropout). Past this, pollRtc() reports failure instead
  // of handing back an ever-more-stale timestamp forever — callers (e.g. the
  // Tamagotchi incubation timer) must be able to tell "time unknown" from "time frozen".
  static constexpr unsigned long MAX_STALE_MS = 5 * CLOCK_POLL_MS;  // 50 seconds

  // Shared cache-checked RTC read used by getTime() and getDate().
  // Returns false if the RTC is absent, or unset with nothing cached yet.
  bool pollRtc(Rtc::DateTime& out) const;

 public:
  // Call after BoardConfig has selected the active device.
  void begin();

  // True if an RTC is present on this device
  bool isAvailable() const { return _available; }

  // Get current hour (0-23) and minute (0-59).
  // Returns false if RTC is not available.
  bool getTime(uint8_t& hour, uint8_t& minute) const;

  // Get the current local calendar date as YYYYMMDD (e.g. 20260806), applying the
  // given UTC offset (same biased quarter-hour encoding as formatTime: 48 = UTC+0).
  // Returns false if the RTC is absent, or present but reports itself unset (oscillator
  // stopped / never programmed) with nothing cached yet — callers should treat that as
  // "date unknown" rather than assuming a date.
  bool getDate(int& yyyymmdd, uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Format time into a caller-provided buffer.
  // 24h mode produces "HH:MM" (needs >=6 bytes); 12h mode produces "H:MM AM"/"HH:MM PM" (needs >=9 bytes).
  // utcOffsetQuarterHoursBiased: biased quarter-hour offset (48 = UTC+0, 0 = UTC-12, 104 = UTC+14).
  // use12Hour: when true, format as 12-hour clock with AM/PM suffix.
  // Returns false if RTC is not available.
  bool formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48, bool use12Hour = false) const;

  // Sync the RTC from an NTP server. Requires WiFi to be connected.
  // Blocks for up to ~5s while waiting for SNTP response.
  // Returns true if the RTC was successfully updated.
  //
  // Debouncing (skip if already synced once) is enforced by the caller, not here,
  // so the HAL stays free of any app-layer settings dependency.
  bool syncFromNTP();
};
