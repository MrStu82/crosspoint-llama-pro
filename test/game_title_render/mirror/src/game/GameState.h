#pragma once
// Shadow of src/game/GameState.h. GameTitleActivity::render() (the harness's actual runtime
// path) never touches GAME_STATE at all -- only GameTitleActivity::loop() does
// (GAME_STATE.hasSaveFile()/newGame() gating the tap-to-start transition into GameActivity),
// and loop() is never called by this harness (main() calls render() only). But
// GameTitleActivity.cpp is one translation unit compiled unmodified, so that dead branch
// still has to compile+link -- and the real GameState.cpp's newGame() pulls in
// AchievementBus.cpp + FlavorTextTracker (src/game/FlavorText.cpp), which pull in the rest of
// the Phase 9 achievement subsystem. None of that has any bearing on rendered pixels here.
// Trivial inline bodies (same pattern as GameActivity.h) keep everything local to this header
// so no GameState.cpp/AchievementBus.cpp/FlavorText.cpp linkage is required. Doesn't touch
// GameTitleActivity's own real draw calls or fonts in any way.

#include <cstdint>

class GameState {
 public:
  static GameState& getInstance() {
    static GameState instance;
    return instance;
  }

  bool hasSaveFile() const { return true; }  // pretend a save exists -> newGame() never runs
  void newGame(uint32_t) {}
};

#define GAME_STATE GameState::getInstance()
