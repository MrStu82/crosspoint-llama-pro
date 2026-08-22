#include "AchievementBus.h"
#include "Achievements.h"
#include "GameState.h"
#include "GameTypes.h"
#include "HalStorage.h"

#include <cstdio>

int main() {
  AchievementBus& bus = ACHIEVEMENTS;
  bus.load();
  HalStorage::resetWriteOpenCalls();

  game::GameEvent kill{};
  kill.type = game::GameEventType::MonsterKilled;
  for (int attempt = 0; attempt < 500 && !bus.isUnlocked(game::AchievementId::FirstBlood); attempt++) {
    bus.resetRun();
    bus.emit(kill);
  }

  if (!bus.isUnlocked(game::AchievementId::FirstBlood)) {
    std::puts("FAIL: FirstBlood was never drawn/unlocked");
    return 1;
  }
  if (HalStorage::writeOpenCalls != 0) {
    std::printf("FAIL: combat emit attempted %d synchronous write(s)\n", HalStorage::writeOpenCalls);
    return 1;
  }
  if (bus.flush()) {
    std::puts("FAIL: stub flush unexpectedly succeeded");
    return 1;
  }
  if (HalStorage::writeOpenCalls != 1) {
    std::printf("FAIL: safe-boundary flush attempted %d writes, expected 1\n", HalStorage::writeOpenCalls);
    return 1;
  }
  if (bus.flush() || HalStorage::writeOpenCalls != 2) {
    std::puts("FAIL: failed flush did not retain dirty state for retry");
    return 1;
  }

  std::puts("Achievement deferred-save harness: PASS (combat writes=0, safe flush=1, failed flush retries)");
  return 0;
}
