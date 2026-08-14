#include "StatsManager.h"

#include <string.h>

bool migrateStatsBlob(const uint8_t* buf, size_t len, GlobalStats& out, bool& needsSave) {
  needsSave = false;

  if (len == sizeof(uint8_t) + sizeof(GlobalStats) && buf[0] == STATS_FILE_VERSION) {
    memcpy(&out, buf + 1, sizeof(GlobalStats));
    return true;
  }

  if (len == sizeof(GlobalStatsV1)) {
    GlobalStatsV1 legacy{};
    memcpy(&legacy, buf, sizeof(GlobalStatsV1));

    out = GlobalStats{};
    out.totalPagesRead = legacy.totalPagesRead;
    out.booksOpened = legacy.booksOpened;
    out.totalReadingTimeSeconds = legacy.totalReadingTimeSeconds;
    out.readingTimeTodaySeconds = legacy.readingTimeTodaySeconds;
    out.pagesReadToday = legacy.pagesReadToday;
    out.booksFinished = legacy.booksFinished;
    out.lastActiveDate = legacy.lastActiveDate;
    for (int i = 0; i < STATS_V1_DAILY_HISTORY_SLOTS; i++) {
      out.dailyHistory[i] = legacy.dailyHistory[i];
    }
    // Legacy files never had a trailing writeIndex worth trusting for the new layout: the
    // 7 migrated days now occupy slots 0..6, so the next write must land at slot 7, not 0
    // (which would immediately clobber a just-migrated day a full cycle early).
    out.dailyHistoryWriteIndex = STATS_V1_DAILY_HISTORY_SLOTS % DAILY_HISTORY_SLOTS;
    needsSave = true;
    return true;
  }

  return false;
}
