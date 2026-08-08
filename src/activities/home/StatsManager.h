#pragma once
#include <stdint.h>
#include <stddef.h>

// Size of the rolling per-day reading history used by the stats history views (12-week
// contribution grid = 84 days). Was 7 pre-v2; see GlobalStatsV1/migrateStatsBlob below for
// the on-disk migration from the old 7-slot format.
static constexpr int DAILY_HISTORY_SLOTS = 84;

// stats.bin format version. Written as a single leading byte ahead of the raw GlobalStats
// blit (see StatsManager::save()/load()). Pre-v2 files had no version byte at all — see
// GlobalStatsV1 and migrateStatsBlob().
static constexpr uint8_t STATS_FILE_VERSION = 2;

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

// Byte-exact layout of every stats.bin ever written before STATS_FILE_VERSION existed —
// no version byte, no header, 7-slot history. Never change this struct; it exists solely
// so migrateStatsBlob() can parse legacy files field-by-field instead of raw-blitting them
// into the (differently sized) current GlobalStats. See migrateStatsBlob() for why a raw
// blit is unsafe here.
static constexpr int STATS_V1_DAILY_HISTORY_SLOTS = 7;
struct GlobalStatsV1 {
  uint32_t totalPagesRead = 0;
  uint32_t booksOpened = 0;
  uint32_t totalReadingTimeSeconds = 0;
  uint32_t readingTimeTodaySeconds = 0;
  uint32_t pagesReadToday = 0;
  uint32_t booksFinished = 0;
  int lastActiveDate = 0;
  DailyMinutesEntry dailyHistory[STATS_V1_DAILY_HISTORY_SLOTS];
  uint8_t dailyHistoryWriteIndex = 0;
};

// Asserted, not assumed: both sizes depend on compiler struct padding. If a toolchain
// change ever shifts either size, this fails the build instead of silently mis-detecting
// file format in migrateStatsBlob().
static_assert(sizeof(GlobalStatsV1) == 88, "legacy stats.bin layout changed size");
static_assert(sizeof(GlobalStats) == 704, "current stats.bin struct layout changed size");

// Parses a raw stats.bin blob (as read from disk, no version byte stripped) into `out`.
// Pure/no I/O — host-testable. Handles three cases:
//   - Current format (1 version byte + sizeof(GlobalStats), byte 0 == STATS_FILE_VERSION):
//     fast-path memcpy, `needsSave` left false.
//   - Legacy v1 format (exactly sizeof(GlobalStatsV1), no version byte): field-by-field
//     migration into `out` — all 7 legacy days copied into slots 0..6, writeIndex set to 7
//     (not 0) so the next write doesn't immediately overwrite a just-migrated day.
//     `needsSave` set true so the caller persists the migrated data in the new format.
//   - Anything else (corrupt/unknown size): returns false, `out`/`needsSave` untouched.
bool migrateStatsBlob(const uint8_t* buf, size_t len, GlobalStats& out, bool& needsSave);

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
