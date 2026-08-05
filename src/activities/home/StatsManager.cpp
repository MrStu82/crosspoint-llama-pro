#include "StatsManager.h"

#include <HalStorage.h>

#include <ctime>

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
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  if (timeinfo.tm_year < 100) {
    // Time not synced yet (ESP32 defaults to 1970). Returning 0 means day rollover
    // won't trigger cleanly until the clock syncs, but the alternative is logging
    // everything against 1970-01-01.
    return 0;
  }

  return (timeinfo.tm_year + 1900) * 10000 + (timeinfo.tm_mon + 1) * 100 + timeinfo.tm_mday;
}

void StatsManager::checkDateReset() {
  const int today = getCurrentDate();
  if (today == 0) return;  // Time not set yet.

  if (stats.lastActiveDate == 0 || stats.lastActiveDate != today) {
    stats.readingTimeTodaySeconds = 0;
    stats.pagesReadToday = 0;
    stats.lastActiveDate = today;
    dirty = true;
  }
}
