#pragma once
// Shadow of src/CrossPointSettings.h for the corrupt-save-notice render harness. Same
// "linked but never actually invoked at runtime for the tested path" technique as
// test/sponsor_hp_clamp/stubs_settings/CrossPointSettings.h -- extended with the extra
// enums/members UITheme.h/UITheme.cpp/BaseTheme.h reference (Edge, UI_THEME, uiTheme) that
// the sponsor_hp_clamp stub didn't need. GameRenderer.cpp's own two SETTINGS references
// (gameTheme, getRefreshFrequency()) live only in draw(), which the harness never calls --
// drawCorruptSaveNotice() is the tested path. UITheme's constructor (via the real, unmodified
// UITheme.cpp's global `UITheme UITheme::instance;`) DOES run SETTINGS.uiTheme ->
// setTheme(...) for real at static-init time, so uiTheme's value here picks which of the
// four stub theme constructors (test/corrupt_save_notice_render/stub_themes/) actually runs.
#include <cstdint>

class CrossPointSettings {
 public:
  static CrossPointSettings& getInstance() {
    static CrossPointSettings instance;
    return instance;
  }

  enum Edge { TOP = 0, BOTTOM = 1 };
  enum UI_THEME { CLASSIC = 0, LYRA = 1, LYRA_3_COVERS = 2, ROUNDEDRAFF = 3 };
  enum GAME_THEME { GAME_THEME_DEFAULT = 0, GAME_THEME_DUNGEON_CRAWLER_CARL = 1, GAME_THEME_COUNT };

  uint8_t uiTheme = CLASSIC;
  uint8_t gameTheme = GAME_THEME_DEFAULT;

  int getRefreshFrequency() const { return 15; }

  // Minimal shadow of the real StatusBarSpec (src/CrossPointSettings.h) -- only the fields
  // UITheme::getStatusBarHeight/getProgressBarHeight actually read. Defaults produce an
  // all-hidden bar (no status/progress bar reserved), which is a real, valid device
  // configuration -- not a special-cased bypass -- so getScreenSafeArea()'s real,
  // unmodified arithmetic still runs for real, it just resolves to zero bar height.
  struct StatusBarSpec {
    bool showChapterPageCount = false;
    bool showBookProgressPercent = false;
    uint8_t titleMode = 0;
    bool showBattery = false;
    bool showBatteryPercent = false;
    uint8_t clockMode = 0;
    bool clock12h = false;
    uint8_t clockUtcOffsetQ = 48;
    uint8_t progressBarMode = 0;
    uint8_t progressBarHeightPx = 0;
    uint8_t xtcMode = 0;

    bool showsProgressBar() const { return progressBarMode != 0; }
    bool showsTitle() const { return titleMode != 0; }
    bool showsClock() const { return clockMode != 0; }
    bool textLaneVisible(bool clockAvailable) const {
      return showChapterPageCount || showBookProgressPercent || showsTitle() || showBattery ||
             (showsClock() && clockAvailable);
    }
  };

  StatusBarSpec statusBarSpec(Edge) const { return StatusBarSpec{}; }
};
#define SETTINGS CrossPointSettings::getInstance()
