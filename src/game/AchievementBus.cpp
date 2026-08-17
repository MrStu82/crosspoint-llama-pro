#include "AchievementBus.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdio>
#include <cstring>

#include "GameState.h"
#include "GameTypes.h"

namespace {
// v1: version byte, count byte, N unlocked bools.
// v2: v1 layout, plus lifetimeTilesWalked (u32), floorsExploredFully (u8),
// magpiePickupCount (u32). Version-gated so an old v1 file still loads
// cleanly (extra counters just stay at their zero default).
constexpr uint8_t ACHIEVEMENTS_FILE_VERSION = 2;
constexpr char ACHIEVEMENTS_DIR[] = "/.crosspoint/game";
constexpr char ACHIEVEMENTS_FILE[] = "/.crosspoint/game/achievements.bin";
constexpr uint8_t COUNT = static_cast<uint8_t>(game::AchievementId::Count);
constexpr uint8_t COMPLETIONIST_THRESHOLD = 40;
}  // namespace

AchievementBus AchievementBus::instance;

void AchievementBus::load() {
  for (auto& u : unlocked) u = false;
  lifetimeTilesWalked = 0;
  floorsExploredFully = 0;
  magpiePickupCount = 0;

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

  if (version >= 2) {
    serialization::readPod(file, lifetimeTilesWalked);
    serialization::readPod(file, floorsExploredFully);
    serialization::readPod(file, magpiePickupCount);
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
  serialization::writePod(file, lifetimeTilesWalked);
  serialization::writePod(file, floorsExploredFully);
  serialization::writePod(file, magpiePickupCount);
  return true;
}

void AchievementBus::resetRun() {
  for (auto& u : unlockedThisRun) u = false;
  wasCriticalThisFloor = false;
  tookDamageThisFloor = false;
  killedAnythingThisFloor = false;
  turnAtCurrentFloorStart = 0;
  turnAtFloorEntry = 0;
  depthAtRecentFloorStart = 0;
  floorsClearedThisWindow = 0;
  hasDiedThisRun = false;
  wasCriticalThisRun = false;
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
  } else {
    snprintf(pendingFlavors_[pendingCount_], sizeof(pendingFlavors_[pendingCount_]), "%s", flavorText);
    pendingCount_++;
  }

  if (id != game::AchievementId::Completionist) {
    uint8_t total = 0;
    for (uint8_t i = 0; i < COUNT; i++) {
      if (unlocked[i]) total++;
    }
    if (total >= COMPLETIONIST_THRESHOLD) {
      unlock(game::AchievementId::Completionist,
             "Achievement: Completionist (Unlocked forty other achievements. This one was inevitable.)");
    }
  }
}

const char* AchievementBus::consumeNewUnlockFlavor() {
  if (pendingCount_ == 0) {
    lastUnlockFlavor_[0] = '\0';
    return lastUnlockFlavor_;
  }
  snprintf(lastUnlockFlavor_, sizeof(lastUnlockFlavor_), "%s", pendingFlavors_[0]);
  // Shift remaining entries down -- pendingCount_ is bounded at
  // MAX_PENDING_UNLOCKS (8), so this is a handful of byte copies at most.
  // memmove (not snprintf) since the two rows are adjacent same-typed array
  // elements -- avoids a spurious -Wrestrict warning from a %s copy where the
  // compiler can't statically prove non-overlap between rows of the same array.
  for (uint8_t i = 1; i < pendingCount_; i++) {
    memmove(pendingFlavors_[i - 1], pendingFlavors_[i], sizeof(pendingFlavors_[i - 1]));
  }
  pendingCount_--;
  return lastUnlockFlavor_;
}

void AchievementBus::addTilesWalked(uint32_t n) {
  lifetimeTilesWalked += n;
  if (lifetimeTilesWalked >= 2000) {
    unlock(game::AchievementId::Wanderer, "Achievement: Wanderer (Walked 2000 tiles across all runs.)");
  }
  if (lifetimeTilesWalked >= 10000) {
    unlock(game::AchievementId::Pathfinder, "Achievement: Pathfinder (Walked 10000 tiles across all runs.)");
  }
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
      if (event.newLevel >= 5) {
        unlock(AchievementId::Veteran, "Achievement: Veteran (Reached character level 5.)");
      }
      if (event.newLevel >= 10) {
        unlock(AchievementId::Seasoned, "Achievement: Seasoned (Reached character level 10.)");
        if (!hasDiedThisRun) {
          unlock(AchievementId::TheUnkilled, "Achievement: The Unkilled (Reached character level 10 without dying.)");
        }
      }
      // Level-up fully heals the player (see GameActivity's level-up handling) --
      // this is the only reliable "back to full hp" signal available from the
      // existing event vocabulary, since there is no dedicated heal event.
      if (wasCriticalThisRun && GAME_STATE.player.hp >= GAME_STATE.player.maxHp) {
        unlock(AchievementId::BackFromTheBrink, "Achievement: Back From The Brink (Healed from below 10% to full in one run.)");
        wasCriticalThisRun = false;
      }
      break;

    case GameEventType::PlayerDamaged:
      wasCriticalThisFloor = (event.maxHp > 0 && event.hpAfter * 10 < event.maxHp);
      tookDamageThisFloor = true;
      if (wasCriticalThisFloor) {
        wasCriticalThisRun = true;
      }
      if (event.hpAfter == 1) {
        unlock(AchievementId::OneHitPoint, "Achievement: One Hit Point (Survived a turn at exactly 1 HP.)");
      }
      break;

    case GameEventType::FloorChanged: {
      if (wasCriticalThisFloor) {
        unlock(AchievementId::ThatllBuffOut, "Achievement: That'll Buff Out (You should be dead. You're welcome.)");
      }
      unlock(AchievementId::FirstSteps, "Achievement: First Steps (Entered the dungeon at all. The bar was on the floor.)");
      if (GAME_STATE.player.dungeonDepth >= 5 && GAME_STATE.player.turnCount < 150) {
        unlock(AchievementId::SpeedRunner, "Achievement: Speed Runner (The System hasn't even finished the ads yet.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 10) {
        unlock(AchievementId::DeepDiver, "Achievement: Deep Diver (Floor 10. The mines get quieter down here.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 2) {
        unlock(AchievementId::DownWeGo, "Achievement: Down We Go (Reached dungeon level 2.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 5) {
        unlock(AchievementId::GettingComfortable, "Achievement: Getting Comfortable (Reached dungeon level 5.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 15) {
        unlock(AchievementId::DeepDelver, "Achievement: Deep Delver (Reached dungeon level 15.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 20) {
        unlock(AchievementId::PressureTolerance, "Achievement: Pressure Tolerance (Reached dungeon level 20.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 25) {
        unlock(AchievementId::AbyssWalker, "Achievement: Abyss-Walker (Reached dungeon level 25.)");
      }
      if (GAME_STATE.player.dungeonDepth >= 22) {
        unlock(AchievementId::StructurallyUnsound, "Achievement: Structurally Unsound (Reached dungeon level 22.)");
      }
      if (GAME_STATE.player.dungeonDepth >= game::MAX_DEPTH) {
        unlock(AchievementId::DungeonSovereign, "Achievement: Dungeon Sovereign (Reached the deepest level the dungeon has.)");
      }
      if (GAME_STATE.player.turnCount >= 1000) {
        unlock(AchievementId::LongHaul, "Achievement: Long Haul (Survived 1000 turns in a single run.)");
      }
      if (GAME_STATE.player.turnCount >= 5000) {
        unlock(AchievementId::Attrition, "Achievement: Attrition (Survived 5000 turns in a single run.)");
      }

      // Express Descent: rolling non-overlapping 3-floor window.
      floorsClearedThisWindow++;
      if (floorsClearedThisWindow >= 3) {
        if (GAME_STATE.player.turnCount - turnAtFloorEntry < 200) {
          unlock(AchievementId::ExpressDescent, "Achievement: Express Descent (Descended three levels in under 200 turns.)");
        }
        floorsClearedThisWindow = 0;
        turnAtFloorEntry = GAME_STATE.player.turnCount;
        depthAtRecentFloorStart = GAME_STATE.player.dungeonDepth;
      }

      // Untouched / Pacifist Run reflect the floor just departed.
      if (killedAnythingThisFloor && !tookDamageThisFloor) {
        unlock(AchievementId::Untouched, "Achievement: Untouched (Cleared a floor without taking a single point of damage.)");
      }
      if (!killedAnythingThisFloor) {
        unlock(AchievementId::PacifistRun, "Achievement: Pacifist Run (Descended a full floor without killing anything.)");
      }

      // Cartographer / Thorough / Obsessive: plain explored-vs-walkable tally,
      // populated by GameActivity before this event is emitted.
      if (event.floorFullyExplored) {
        if (floorsExploredFully < 255) floorsExploredFully++;
        unlock(AchievementId::Cartographer, "Achievement: Cartographer (Fully explored a floor.)");
        if (floorsExploredFully >= 5) {
          unlock(AchievementId::Thorough, "Achievement: Thorough (Fully explored five floors.)");
        }
        if (floorsExploredFully >= 20) {
          unlock(AchievementId::Obsessive, "Achievement: Obsessive (Fully explored twenty floors.)");
        }
      }

      // Shortcut / Scenic Route: per-floor turn timer.
      uint32_t floorTurns = GAME_STATE.player.turnCount - turnAtCurrentFloorStart;
      if (floorTurns <= 30) {
        unlock(AchievementId::Shortcut, "Achievement: Shortcut (Found the stairs within 30 turns of arriving.)");
      }
      if (floorTurns > 500) {
        unlock(AchievementId::ScenicRoute, "Achievement: Scenic Route (Took over 500 turns on a single floor.)");
      }
      turnAtCurrentFloorStart = GAME_STATE.player.turnCount;

      wasCriticalThisFloor = false;
      tookDamageThisFloor = false;
      killedAnythingThisFloor = false;
      break;
    }

    case GameEventType::PlayerDied:
      hasDiedThisRun = true;
      unlock(AchievementId::DiedAnyway, "Achievement: Died Anyway (Died. The System is not surprised.)");
      if (event.monsterAttack <= 2) {
        unlock(AchievementId::AudienceParticipation, "Achievement: Audience Participation (Killed by something that shouldn't have been able to.)");
      }
      break;

    case GameEventType::MonsterKilled:
      if (event.cleanSweep) {
        unlock(AchievementId::CleanSweep, "Achievement: Clean Sweep (Cleared an entire floor of monsters.)");
        break;
      }
      killedAnythingThisFloor = true;
      unlock(AchievementId::FirstBlood, "Achievement: First Blood (Killed something. It started it.)");
      if (event.monsterMaxHp > 0 && event.damage >= event.monsterMaxHp * 3) {
        unlock(AchievementId::EscalationOfForce, "Achievement: Escalation of Force (That was more bullet than monster.)");
      }
      if (event.hpBeforeHit > 0 && event.damage > event.hpBeforeHit) {
        unlock(AchievementId::Overkill, "Achievement: Overkill (Dealt more damage in one blow than the target had left.)");
      }
      if (event.monsterMaxHp > game::effectiveMaxHp(GAME_STATE.player)) {
        unlock(AchievementId::GiantKiller, "Achievement: Giant-Killer (Killed something with more life in it than you.)");
      }
      if (GAME_STATE.player.kills >= 10) {
        unlock(AchievementId::Ratcatcher, "Achievement: Ratcatcher (Killed 10 monsters.)");
      }
      if (GAME_STATE.player.kills >= 50) {
        unlock(AchievementId::Exterminator, "Achievement: Exterminator (Killed 50 monsters.)");
      }
      if (GAME_STATE.player.kills >= 250) {
        unlock(AchievementId::Beastbane, "Achievement: Beastbane (Killed 250 monsters.)");
      }
      break;

    case GameEventType::ItemUsed:
      // No seed achievement hangs off this event yet.
      break;

    case GameEventType::ItemPickedUp:
      if (GAME_STATE.inventoryCount >= game::MAX_INVENTORY) {
        unlock(AchievementId::PackRat, "Achievement: Pack Rat (Your pack is bursting. The System charges storage fees.)");
      }
      magpiePickupCount++;
      if (magpiePickupCount >= 1) {
        unlock(AchievementId::FindersKeepers, "Achievement: Finders Keepers (Picked up your first item.)");
      }
      if (magpiePickupCount >= 50) {
        unlock(AchievementId::Magpie, "Achievement: Magpie (Picked up 50 items.)");
      }
      if (GAME_STATE.player.gold >= 1000) {
        unlock(AchievementId::TheWealthy, "Achievement: The Wealthy (Accumulated 1000 gold.)");
      }
      if (GAME_STATE.player.gold >= 10000) {
        unlock(AchievementId::ObsceneWealth, "Achievement: Obscene Wealth (Accumulated 10000 gold.)");
      }
      break;

    case GameEventType::ItemThrown:
      if (event.killedMonster) {
        unlock(AchievementId::PercussiveMaintenance, "Achievement: Percussive Maintenance (You fixed it. By throwing something else at it.)");
      }
      break;

    case GameEventType::LootBoxOpened:
      unlock(AchievementId::SponsoredContent, "Achievement: Sponsored Content (You opened it. Somewhere, a marketing department is applauding.)");
      if (GAME_STATE.player.gold >= 1000) {
        unlock(AchievementId::TheWealthy, "Achievement: The Wealthy (Accumulated 1000 gold.)");
      }
      if (GAME_STATE.player.gold >= 10000) {
        unlock(AchievementId::ObsceneWealth, "Achievement: Obscene Wealth (Accumulated 10000 gold.)");
      }
      break;
  }
}
