#pragma once
#include <stdint.h>

struct GlobalStats {
  uint32_t totalPagesRead = 0;
  uint32_t booksOpened = 0;
  uint32_t totalReadingTimeSeconds = 0;
  uint32_t readingTimeTodaySeconds = 0;
  uint32_t pagesReadToday = 0;
  uint32_t booksFinished = 0;
  int lastActiveDate = 0;  // Format: YYYYMMDD
};

class StatsManager {
  GlobalStats stats;
  bool dirty = false;

  StatsManager();  // Singleton
  void load();

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
};

// Helper macro to access the stats singleton
#define READING_STATS StatsManager::getInstance()
