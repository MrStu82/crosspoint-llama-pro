#include "AchievementBus.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdio>
#include <cstring>

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
  if (pendingCount_ >= MAX_PENDING_UNLOCKS) {
    LOG_ERR("ACH", "pending-unlock queue full, dropping notification for: %s", flavorText);
    return;
  }
  snprintf(pendingFlavors_[pendingCount_], sizeof(pendingFlavors_[pendingCount_]), "%s", flavorText);
  pendingCount_++;
}

const char* AchievementBus::consumeNewUnlockFlavor() {
  if (pendingCount_ == 0) {
    lastUnlockFlavor_[0] = '\0';
    return lastUnlockFlavor_;
  }
  snprintf(lastUnlockFlavor_, sizeof(lastUnlockFlavor_), "%s", pendingFlavors_[0]);
  // Shift remaining entries down -- pendingCount_ is bounded at
  // MAX_PENDING_UNLOCKS (3), so this is a handful of byte copies at most.
  // memmove (not snprintf) since the two rows are adjacent same-typed array
  // elements -- avoids a spurious -Wrestrict warning from a %s copy where the
  // compiler can't statically prove non-overlap between rows of the same array.
  for (uint8_t i = 1; i < pendingCount_; i++) {
    memmove(pendingFlavors_[i - 1], pendingFlavors_[i], sizeof(pendingFlavors_[i - 1]));
  }
  pendingCount_--;
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

    case GameEventType::ItemThrown:
      if (event.killedMonster) {
        unlock(AchievementId::PercussiveMaintenance, "Achievement: Percussive Maintenance (You fixed it. By throwing something else at it.)");
      }
      break;
  }
}
