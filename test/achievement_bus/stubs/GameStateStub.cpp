// Test-only stand-in for GameState.cpp, which pulls in <Arduino.h> and can't
// compile on the host. GameState.h itself has no Arduino dependency (only
// <string>, GameTypes.h, SaveValidity.h), so the real header is used
// unmodified -- only the .cpp implementation is swapped out here, providing
// just the handful of members AchievementBus.cpp actually calls
// (GAME_STATE.player.*, .inventoryCount, .addMessage(), .rollRange()).
// Everything else GameState.h declares (newGame, saveToFile, loadFromFile,
// ...) is never referenced by AchievementBus.cpp and so never needs a body
// here -- non-odr-used member functions don't require a definition to link.

#include "GameState.h"

GameState GameState::instance;

void GameState::addMessage(const char*) {
  // Achievement flavor text goes nowhere in this test -- nothing reads it.
}

// A simple LCG, not the game's real combat RNG stream -- this test isn't
// exercising RNG quality, only AchievementBus's draw-guard + condition logic.
// Deliberately never reseeded/reset across calls (including across
// resetRun()), so repeated resetRun() calls in a single test binary run
// produce different per-run draws, the same way real distinct game runs
// would.
uint32_t GameState::rollRange(uint32_t max) {
  static uint32_t state = 0x2545F491u;
  state = state * 1103515245u + 12345u;
  if (max == 0) return 0;
  return state % max;
}
