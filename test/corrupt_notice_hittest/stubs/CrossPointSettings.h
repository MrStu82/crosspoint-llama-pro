#pragma once
// Shadow of src/CrossPointSettings.h for the sponsor HP-clamp harness. GameRenderer.cpp
// (compiled unmodified, in full, as part of the real GameActivity.cpp it's a data member
// of) calls SETTINGS.getRefreshFrequency() from GameRenderer::draw() -- but the harness's
// call path (GameActivity::onEnter() -> loadOrGenerateLevel()) never calls draw()/render()
// at all, so that call is never actually reached at runtime. The real CrossPointSettings
// pulls in a hardware-coupled PersistableStore<>/BoardConfig.h chain this harness has no
// reason to touch. Same "shadow header, real .cpp never actually reaches it" technique as
// this harness's own GameMenuActivity.h shadow and test/game_title_render/'s treatment of
// GameActivity itself -- see that README's "Why mirror/ exists" section.
class CrossPointSettings {
 public:
  static CrossPointSettings& getInstance() {
    static CrossPointSettings instance;
    return instance;
  }

  int getRefreshFrequency() const { return 15; }

  // Unlike sponsor_hp_clamp's version of this shadow header (which never reaches a
  // drawCorruptSaveNotice()/BaseTheme.h call path), this harness's GameRenderer.cpp
  // is compiled with BaseTheme.h/UITheme.h in its include chain regardless of runtime
  // reachability -- header type errors are compile-time, not conditional on which
  // functions actually get called. Edge/UI_THEME values copied verbatim from the real
  // src/CrossPointSettings.h so BaseTheme.h/UITheme.h's declarations typecheck.
  enum Edge { TOP = 0, BOTTOM = 1 };
  enum UI_THEME { CLASSIC = 0, LYRA = 1, LYRA_3_COVERS = 2, ROUNDEDRAFF = 3 };
  // Real default is LYRA_3_COVERS (src/CrossPointSettings.h:290), not CLASSIC -- confirmed by
  // direct read this session. UITheme::instance's constructor branches on this value, so the
  // stub must match the real default or it silently constructs a different theme than
  // production does.
  uint8_t uiTheme = LYRA_3_COVERS;

  enum GAME_THEME { GAME_THEME_DEFAULT = 0, GAME_THEME_DUNGEON_CRAWLER_CARL = 1, GAME_THEME_COUNT };
  uint8_t gameTheme = GAME_THEME_DEFAULT;

  // Real struct (src/CrossPointSettings.h:355-378) copied verbatim -- UITheme.cpp's status-bar
  // draw path reads fields off this via SETTINGS.statusBarSpec(edge), even though this
  // harness's tested path never reaches it at runtime; the linker still needs it to typecheck.
  static constexpr uint8_t HIDE_TITLE = 0;
  static constexpr uint8_t STATUS_BAR_CLOCK_HIDE = 0;
  static constexpr uint8_t HIDE_PROGRESS = 0;
  static constexpr uint8_t XTC_STATUS_BAR_HIDE = 0;

  struct StatusBarSpec {
    bool showChapterPageCount = false;
    bool showBookProgressPercent = false;
    uint8_t titleMode = HIDE_TITLE;
    bool showBattery = false;
    bool showBatteryPercent = false;
    uint8_t clockMode = STATUS_BAR_CLOCK_HIDE;
    bool clock12h = false;
    uint8_t clockUtcOffsetQ = 48;
    uint8_t progressBarMode = HIDE_PROGRESS;
    uint8_t progressBarHeightPx = 0;
    uint8_t xtcMode = XTC_STATUS_BAR_HIDE;

    bool showsProgressBar() const { return progressBarMode != HIDE_PROGRESS; }
    bool showsTitle() const { return titleMode != HIDE_TITLE; }
    bool showsClock() const { return clockMode != STATUS_BAR_CLOCK_HIDE; }
    bool textLaneVisible(bool clockAvailable) const {
      return showChapterPageCount || showBookProgressPercent || showsTitle() || showBattery ||
             (showsClock() && clockAvailable);
    }
  };
  StatusBarSpec statusBarSpec(Edge) const { return StatusBarSpec(); }
};

#define SETTINGS CrossPointSettings::getInstance()
