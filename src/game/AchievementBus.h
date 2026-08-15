#pragma once

#include "Achievements.h"

class AchievementBus {
  static AchievementBus instance;

  bool unlocked[static_cast<uint8_t>(game::AchievementId::Count)] = {};
  bool wasCriticalThisFloor = false;

 public:
  static AchievementBus& getInstance() { return instance; }

  // Load unlock state from achievements.bin. Absent file == nothing unlocked.
  void load();

  // Game logic calls this and knows nothing about achievement identities.
  void emit(const game::GameEvent& event);

  bool isUnlocked(game::AchievementId id) const;

 private:
  void unlock(game::AchievementId id, const char* flavorText);
  bool save() const;
};

#define ACHIEVEMENTS AchievementBus::getInstance()
