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

  enum GAME_THEME { GAME_THEME_DEFAULT = 0, GAME_THEME_DUNGEON_CRAWLER_CARL = 1, GAME_THEME_COUNT };
  uint8_t gameTheme = GAME_THEME_DEFAULT;
};

#define SETTINGS CrossPointSettings::getInstance()
