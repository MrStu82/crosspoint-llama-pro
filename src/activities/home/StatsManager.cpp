#include "StatsManager.h"

#include <HalClock.h>
#include <HalStorage.h>

#include <ctime>

#include "CrossPointSettings.h"

namespace {
constexpr const char* STATS_FILE_PATH = "/.crosspoint/stats.bin";
}  // namespace

StatsManager::StatsManager() { load(); }

void StatsManager::load() {
  HalFile file;
  if (!Storage.openFileForRead("STM", STATS_FILE_PATH, file)) {
    // Initialize defaults if the file doesn't exist yet.
    stats = GlobalStats{};
    return;
  }

  int readLen = file.read(reinterpret_cast<uint8_t*>(&stats), sizeof(GlobalStats));
  if (readLen <= 0) {
    // Empty or unreadable file.
    stats = GlobalStats{};
  } else if (static_cast<size_t>(readLen) < sizeof(GlobalStats)) {
    // Graceful migration from an older, smaller struct. Missing trailing fields are
    // implicitly 0 from the GlobalStats{} default member initializers above.
    dirty = true;  // Needs saving to expand to the current struct size.
  }

  checkDateReset();
}

void StatsManager::save() {
  if (!dirty) return;
  HalFile file;
  if (Storage.openFileForWrite("STM", STATS_FILE_PATH, file)) {
    file.write(reinterpret_cast<const uint8_t*>(&stats), sizeof(GlobalStats));
    dirty = false;
  }
}

void StatsManager::incrementPagesRead(uint32_t count) {
  checkDateReset();
  stats.totalPagesRead += count;
  stats.pagesReadToday += count;
  dirty = true;
}

void StatsManager::incrementBooksOpened() {
  checkDateReset();
  stats.booksOpened += 1;
  dirty = true;
}

void StatsManager::incrementBooksFinished() {
  checkDateReset();
  stats.booksFinished += 1;
  dirty = true;
}

void StatsManager::addReadingTimeSeconds(uint32_t seconds) {
  checkDateReset();
  stats.totalReadingTimeSeconds += seconds;
  stats.readingTimeTodaySeconds += seconds;
  dirty = true;
}

int StatsManager::getCurrentDate() const {
  // Day boundary = RTC local midnight: read the BM8563 hardware RTC directly (not libc
  // time(), which is only ever set as a side effect of an NTP sync and reverts to 1970
  // on every reboot) and apply the user's configured UTC offset, same as the clock display.
  int yyyymmdd = 0;
  if (!halClock.getDate(yyyymmdd, SETTINGS.clockUtcOffsetQ)) {
    // RTC absent, or present but never set (oscillator stopped) — first boot before the
    // user has synced or set the clock. Returning 0 means day rollover (and the 7-day
    // tracker) won't trigger cleanly until the clock is set; the alternative is logging
    // everything against a fabricated date, which is worse.
    return 0;
  }
  return yyyymmdd;
}

void StatsManager::checkDateReset() {
  const int today = getCurrentDate();
  if (today == 0) return;  // Time not set yet.

  if (stats.lastActiveDate == 0 || stats.lastActiveDate != today) {
    // Archive the day that just ended into the 7-day history before resetting counters.
    // Skip archiving on the very first-ever date set (lastActiveDate == 0) — there's no
    // prior day to record. Zero-minute days are also skipped: they'd just occupy a slot
    // with nothing to show, and getLast7DaysMinutes() already reports 0 for any day with
    // no matching entry.
    if (stats.lastActiveDate != 0 && stats.readingTimeTodaySeconds > 0) {
      DailyMinutesEntry& slot = stats.dailyHistory[stats.dailyHistoryWriteIndex];
      slot.date = stats.lastActiveDate;
      slot.minutes = static_cast<uint16_t>(stats.readingTimeTodaySeconds / 60);
      stats.dailyHistoryWriteIndex = static_cast<uint8_t>((stats.dailyHistoryWriteIndex + 1) % DAILY_HISTORY_SLOTS);
    }
    stats.readingTimeTodaySeconds = 0;
    stats.pagesReadToday = 0;
    stats.lastActiveDate = today;
    dirty = true;
  }
}

int StatsManager::getDateDaysAgo(int daysAgo) const {
  const int today = getCurrentDate();
  if (today == 0) return 0;
  if (daysAgo == 0) return today;

  // Round-trip through mktime so day/month/year carries are calendar-correct across
  // month/year boundaries, same pattern as Rtc::adjust() and HalClock::getDate().
  struct tm t {};
  t.tm_year = today / 10000 - 1900;
  t.tm_mon = (today / 100) % 100 - 1;
  t.tm_mday = today % 100 - daysAgo;
  t.tm_hour = 12;  // noon: keeps the subtraction well clear of any DST edge, though this
                    // board has none (RTC/system TZ is always fixed UTC0 + a manual offset).
  time_t epoch = mktime(&t);
  struct tm norm;
  localtime_r(&epoch, &norm);
  return (norm.tm_year + 1900) * 10000 + (norm.tm_mon + 1) * 100 + norm.tm_mday;
}

void StatsManager::getLast7DaysMinutes(uint16_t outMinutes[DAILY_HISTORY_SLOTS], int* outDates) {
  checkDateReset();
  for (int i = 0; i < DAILY_HISTORY_SLOTS; i++) {
    // Oldest first: slot 0 = 6 days ago, slot 6 = today.
    const int daysAgo = DAILY_HISTORY_SLOTS - 1 - i;
    const int date = getDateDaysAgo(daysAgo);
    if (outDates) outDates[i] = date;

    if (date == 0) {
      outMinutes[i] = 0;
      continue;
    }
    if (daysAgo == 0) {
      outMinutes[i] = static_cast<uint16_t>(stats.readingTimeTodaySeconds / 60);
      continue;
    }
    outMinutes[i] = 0;
    for (const DailyMinutesEntry& entry : stats.dailyHistory) {
      if (entry.date == date) {
        outMinutes[i] = entry.minutes;
        break;
      }
    }
  }
}
