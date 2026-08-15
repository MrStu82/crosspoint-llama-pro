#include "AchievementBus.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdio>

#include "GameState.h"

namespace {
constexpr uint8_t ACHIEVEMENTS_FILE_VERSION = 1;
constexpr char ACHIEVEMENTS_DIR[] = "/.crosspoint/game";
constexpr char ACHIEVEMENTS_FILE[] = "/.crosspoint/game/achievements.bin";
constexpr uint8_t COUNT = static_cast<uint8_t>(game::AchievementId::Count);
}  // namespace

AchievementBus AchievementBus::instance;

void AchievementBus::load() {
  for (auto& u : unlocked) u = false;

  HalFile file;
  if (!Storage.openFileForRead("ACH", ACHIEVEMENTS_FILE, file)) {
    // No file yet == nothing unlocked. Not an error.
    return;
  }

  uint8_t version;
  serialization::readPod(file, version);
  if (version > ACHIEVEMENTS_FILE_VERSION) {
    LOG_ERR("ACH", "Unknown achievements version %u", version);
    return;
  }

  uint8_t storedCount;
  serialization::readPod(file, storedCount);
  uint8_t readCount = storedCount < COUNT ? storedCount : COUNT;
  for (uint8_t i = 0; i < readCount; i++) {
    serialization::readPod(file, unlocked[i]);
  }
}

bool AchievementBus::save() const {
  Storage.mkdir(ACHIEVEMENTS_DIR);

  HalFile file;
  if (!Storage.openFileForWrite("ACH", ACHIEVEMENTS_FILE, file)) {
    LOG_ERR("ACH", "Failed to open achievements file for writing");
    return false;
  }

  serialization::writePod(file, ACHIEVEMENTS_FILE_VERSION);
  serialization::writePod(file, COUNT);
  for (uint8_t i = 0; i < COUNT; i++) {
    serialization::writePod(file, unlocked[i]);
  }
  return true;
}

void AchievementBus::resetRun() {
  for (auto& u : unlockedThisRun) u = false;
}

bool AchievementBus::isUnlocked(game::AchievementId id) const {
  return unlocked[static_cast<uint8_t>(id)];
}

bool AchievementBus::isUnlockedThisRun(game::AchievementId id) const {
  return unlockedThisRun[static_cast<uint8_t>(id)];
}

void AchievementBus::unlock(game::AchievementId id, const char* flavorText) {
  uint8_t idx = static_cast<uint8_t>(id);
  unlockedThisRun[idx] = true;
  if (unlocked[idx]) return;
  unlocked[idx] = true;
  save();
  GAME_STATE.addMessage(flavorText);
  snprintf(lastUnlockFlavor_, sizeof(lastUnlockFlavor_), "%s", flavorText);
  hasNewUnlock_ = true;
}

const char* AchievementBus::consumeNewUnlockFlavor() {
  hasNewUnlock_ = false;
  return lastUnlockFlavor_;
}

void AchievementBus::emit(const game::GameEvent& event) {
  using game::AchievementId;
  using game::GameEventType;

  switch (event.type) {
    case GameEventType::LevelUp:
      unlock(AchievementId::Ding, "Achievement: Ding! (You leveled up. Groundbreaking.)");
      if (event.newLevel >= 20) {
        unlock(AchievementId::MaxedOut, "Achievement: Maxed Out (Level 20. The System raises your ad rates.)");
      }
      break;

    case GameEventType::PlayerDamaged:
      wasCriticalThisFloor = (event.maxHp > 0 && event.hpAfter * 10 < event.maxHp);
      break;

    case GameEventType::FloorChanged:
      if (wasCriticalThisFloor) {
        unlock(AchievementId::ThatllBuffOut, "Achievement: That'll Buff Out (You should be dead. You're welcome.)");
      }
      wasCriticalThisFloor = false;
      if (GAME_STATE.player.dungeonDepth >= 5 && GAME_STATE.player.turnCount < 150) {
        unlock(AchievementId::SpeedRunner, "Achievement: Speed Runner (The System hasn't even finished the ads yet.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 10) {
        unlock(AchievementId::DeepDiver, "Achievement: Deep Diver (Floor 10. The mines get quieter down here.)");
      }
      break;

    case GameEventType::PlayerDied:
      if (event.monsterAttack <= 2) {
        unlock(AchievementId::AudienceParticipation, "Achievement: Audience Participation (Killed by something that shouldn't have been able to.)");
      }
      break;

    case GameEventType::MonsterKilled:
      if (event.monsterMaxHp > 0 && event.damage >= event.monsterMaxHp * 3) {
        unlock(AchievementId::EscalationOfForce, "Achievement: Escalation of Force (That was more bullet than monster.)");
      }
      break;

    case GameEventType::ItemUsed:
      // No seed achievement hangs off this event yet.
      break;

    case GameEventType::ItemPickedUp:
      if (GAME_STATE.inventoryCount >= game::MAX_INVENTORY) {
        unlock(AchievementId::PackRat, "Achievement: Pack Rat (Your pack is bursting. The System charges storage fees.)");
      }
      break;
  }
}
