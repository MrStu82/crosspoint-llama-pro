#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

class CrossPointState {
 public:
  static constexpr uint8_t SLEEP_RECENT_COUNT = 16;
  std::string openEpubPath;
  std::string favoriteSleepImagePath;
  uint8_t recentSleepFill = 0;
  uint8_t recentOverlaySleepFill = 0;
  bool lastSleepFromReader = false;
  bool isRecentSleep(uint16_t, uint8_t) const { return false; }
  bool isRecentOverlaySleep(uint16_t, uint8_t) const { return false; }
  void pushRecentSleep(uint16_t) { recentSleepFill = std::min<uint8_t>(SLEEP_RECENT_COUNT, recentSleepFill + 1); }
  void pushRecentOverlaySleep(uint16_t) {
    recentOverlaySleepFill = std::min<uint8_t>(SLEEP_RECENT_COUNT, recentOverlaySleepFill + 1);
  }
  bool saveToFile() const { return true; }
};

inline CrossPointState testState;
#define APP_STATE testState
