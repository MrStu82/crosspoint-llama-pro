#pragma once
// Test stub. This is a LOGIC/control-flow test (whole-run corrupt-save notice
// -> resolveWholeRunCorruptNotice()), not a pixel-rendering test -- the real
// GameRenderer (full font/EPD draw pipeline, see test/sponsor_hp_clamp/ for
// the precedent that links it for real) is deliberately not linked here.
// GameActivity.h embeds a `GameRenderer gameRenderer` member BY VALUE, so
// this stub must be a complete, default-constructible type with the exact
// public method surface GameActivity.cpp actually calls: confirmed via grep
// to be only init, drawCorruptSaveNotice, drawEndScreen, draw,
// hitTestControls, showNotification. All bodies are inert no-ops.
//
// EndScreenData/NotificationKind are copied verbatim from the real
// src/game/GameRenderer.h (GameActivity.h/.cpp reference both types
// directly, e.g. the `endScreenData` member and showNotification() calls) --
// game::AchievementId is pulled in for real (src/game/Achievements.h is
// header-only, confirmed via read, so no stub needed for it).
#include "game/Achievements.h"
#include "game/GameTypes.h"
#include "MappedInputManager.h"

enum class NotificationKind {
  LevelUp,
  Achievement,
  FloorEntry,
  BossArrival,
  Death,
};

struct EndScreenData {
  char cause[32] = "";
  uint8_t floor = 0;
  uint32_t turns = 0;
  uint16_t kills = 0;
  uint8_t level = 0;
  game::AchievementId unlockedIds[static_cast<uint8_t>(game::AchievementId::Count)];
  uint8_t unlockedCount = 0;
};

class GfxRenderer;

class GameRenderer {
 public:
  void init(GfxRenderer&) {}

  void draw(GfxRenderer&, const game::Tile*, const uint8_t*, const game::Monster*, uint8_t, const game::Item*,
            uint8_t, const bool*) {}

  bool hitTestControls(int, int, MappedInputManager::Button&) const { return false; }

  void drawEndScreen(GfxRenderer&, bool, const EndScreenData&) const {}

  void drawCorruptSaveNotice(GfxRenderer&, bool, uint8_t, uint8_t) const {}

  void showNotification(NotificationKind, const char*) {}
};
