#pragma once
#include <stdint.h>

// Size of the rolling per-day reading history used by the 7-day tracker.
static constexpr int DAILY_HISTORY_SLOTS = 7;

struct DailyMinutesEntry {
  int date = 0;        // YYYYMMDD, 0 = empty slot
  uint16_t minutes = 0;
};

struct GlobalStats {
  uint32_t totalPagesRead = 0;
  uint32_t booksOpened = 0;
  uint32_t totalReadingTimeSeconds = 0;
  uint32_t readingTimeTodaySeconds = 0;
  uint32_t pagesReadToday = 0;
  uint32_t booksFinished = 0;
  int lastActiveDate = 0;  // Format: YYYYMMDD

  // Ring buffer of the last DAILY_HISTORY_SLOTS distinct calendar days that had any
  // reading activity, written one slot per day on day rollover (see
  // StatsManager::checkDateReset()). Today's own minutes live in
  // readingTimeTodaySeconds until the day rolls over, not here — see
  // getLast7DaysMinutes().
  DailyMinutesEntry dailyHistory[DAILY_HISTORY_SLOTS];
  uint8_t dailyHistoryWriteIndex = 0;
};

class StatsManager {
  GlobalStats stats;
  bool dirty = false;

  StatsManager();  // Singleton
  void load();

  // YYYYMMDD for `daysAgo` calendar days before today (0 = today), per the RTC and the
  // user's configured UTC offset. Returns 0 if the date is unknown (RTC unset).
  int getDateDaysAgo(int daysAgo) const;

 public:
  static StatsManager& getInstance() {
    static StatsManager instance;
    return instance;
  }

  void save();
  void checkDateReset();
  int getCurrentDate() const;

  void incrementPagesRead(uint32_t count = 1);
  void incrementBooksOpened();
  void incrementBooksFinished();
  void addReadingTimeSeconds(uint32_t seconds);

  const GlobalStats& getStats() const { return stats; }

  // Fills outMinutes[0..6] with minutes read on each of the last 7 calendar days,
  // oldest first (outMinutes[6] is today). outDates, if non-null, receives the matching
  // YYYYMMDD for each slot (0 if the date itself is unknown, e.g. RTC unset).
  void getLast7DaysMinutes(uint16_t outMinutes[DAILY_HISTORY_SLOTS], int* outDates = nullptr);
};

// Helper macro to access the stats singleton
#define READING_STATS StatsManager::getInstance()
