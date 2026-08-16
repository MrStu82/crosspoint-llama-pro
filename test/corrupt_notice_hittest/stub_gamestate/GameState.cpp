// Empty definitions for GameState's out-of-line methods, satisfying the linker only.
// GameRenderer.cpp (compiled unmodified, in full) references GAME_STATE.getMessage()/
// .player/.addMessage() etc. in code paths this harness never executes (it only calls
// GameRenderer::initForTest() and GameRenderer::hitTestCorruptSaveNoticeContinue(), both
// pure geometry with zero GameState access per GameRenderer.h's own contract) -- but since
// GameRenderer.cpp is one translation unit, every symbol any of its (even unreached)
// functions reference must still resolve at link time. The real GameState.cpp pulls in
// HalStorage/save-file I/O this harness has no reason to touch -- same
// "shadow .cpp, real .h never actually reaches it at runtime" technique as
// stub_themes/BaseTheme.cpp.
#include "game/GameState.h"

GameState GameState::instance;

void GameState::newGame(uint32_t) {}
void GameState::addMessage(const char*) {}
uint32_t GameState::rollRange(uint32_t) { return 0; }
int GameState::rollRangeInclusive(int min, int) { return min; }

const std::string& GameState::getMessage(int) const {
  static const std::string empty;
  return empty;
}

bool GameState::saveToFile() const { return false; }
bool GameState::loadFromFile() { return false; }
bool GameState::hasSaveFile() const { return false; }
void GameState::deleteSaveFile() const {}

SaveValidity GameState::validateSaveFile() { return SaveValidity{}; }
