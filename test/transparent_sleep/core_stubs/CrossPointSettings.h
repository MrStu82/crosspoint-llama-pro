#pragma once

#include <cstdint>

class CrossPointSettings {
 public:
  enum SLEEP_SCREEN_MODE {
    DARK = 0,
    LIGHT = 1,
    CUSTOM = 2,
    COVER = 3,
    COVER_CUSTOM = 4,
    BLANK = 5,
    QUICK_RESUME = 6,
    TRANSPARENT_CUSTOM = 7,
  };
  enum SLEEP_SCREEN_COVER_MODE { FIT = 0, CROP = 1 };
  enum SLEEP_SCREEN_COVER_FILTER { NO_FILTER = 0, BLACK_AND_WHITE = 1, INVERTED_BLACK_AND_WHITE = 2 };
  enum QUICK_RESUME_SLEEP_SCREEN { QUICK_RESUME_NEVER = 0, QUICK_RESUME_AFTER_TIMEOUT = 1 };

  uint8_t sleepScreen = DARK;
  uint8_t sleepScreenCoverMode = FIT;
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  uint8_t quickResumeSleepScreen = QUICK_RESUME_NEVER;
  uint8_t orientation = 0;
};

inline CrossPointSettings testSettings;
#define SETTINGS testSettings
