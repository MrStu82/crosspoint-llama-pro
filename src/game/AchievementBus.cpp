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
  persistenceDirty_ = false;
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

bool AchievementBus::flush() {
  if (!persistenceDirty_) return true;
  if (!save()) return false;
  persistenceDirty_ = false;
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

  for (auto& c : conditionEventCounts_) c = 0;
  for (auto& b : conditionWindowBroken_) b = false;

  // Draw this run's random subset of the WHOLE achievement id space (the
  // original 48 hand-coded ids included -- parent's correction 2026-08-18:
  // they fold into the pool and take their chances like everything else, so
  // this draws from ids [0, COUNT), not just game::CONDITIONS' data-driven
  // rows) via a partial Fisher-Yates shuffle, using the same combat RNG
  // stream every other run-scoped roll uses so the draw is deterministic per
  // game seed.
  game::AchievementId pool[COUNT];
  for (uint8_t i = 0; i < COUNT; i++) pool[i] = static_cast<game::AchievementId>(i);
  uint8_t drawSize = COUNT < DRAWN_POOL_SIZE ? COUNT : DRAWN_POOL_SIZE;
  for (uint8_t i = 0; i < drawSize; i++) {
    uint32_t j = i + GAME_STATE.rollRange(COUNT - i);
    game::AchievementId tmp = pool[i];
    pool[i] = pool[j];
    pool[j] = tmp;
    drawnThisRun_[i] = pool[i];
  }
  drawnCount_ = drawSize;
}

bool AchievementBus::isDrawnThisRun(game::AchievementId id) const {
  for (uint8_t i = 0; i < drawnCount_; i++) {
    if (drawnThisRun_[i] == id) return true;
  }
  return false;
}

bool AchievementBus::isUnlocked(game::AchievementId id) const {
  return unlocked[static_cast<uint8_t>(id)];
}

bool AchievementBus::isUnlockedThisRun(game::AchievementId id) const {
  return unlockedThisRun[static_cast<uint8_t>(id)];
}

void AchievementBus::unlock(game::AchievementId id, const char* flavorText) {
  uint8_t idx = static_cast<uint8_t>(id);
  // The draw guard: an id that wasn't dealt into this run's 50-slot draw
  // cannot fire, no matter what its condition row (or emit()'s hand-coded
  // switch, for the legacy 48) says. FIRST_DRAWABLE_ID is 0 -- every id,
  // legacy and data-driven alike, is equally subject to this (parent's
  // correction 2026-08-18: "they just stop being special").
  if (idx >= FIRST_DRAWABLE_ID && !isDrawnThisRun(id)) {
    return;
  }
  // Run-scoped mechanical reward: a Buff/Skill grants a real, point-of-use
  // stat modifier (same pattern as Sponsors -- never mutates a base stat
  // directly) for THIS run only, per parent's ruling 2026-08-18. Achievements
  // themselves reset per run (that's what the 50-id draw in resetRun() is
  // for) so the reward must too, or a run-thirty player is carrying decades
  // of unearned stats and the roguelike stops being one. It fires on every
  // run this id is drawn and earned, not just the first time ever --
  // grantBuff/grantSkill already no-op if the id is already active, so this
  // can't double-grant mid-run. Player::activeBuffIds/activeSkillIds are
  // wiped on every GameState::newGame() (player = game::Player{}), so
  // nothing here outlives the run it was earned in; what persists across
  // runs is only the lifetime unlocked[] record below (and the progress
  // screen it drives), never the numbers.
  if (!unlockedThisRun[idx]) {
    const game::AchievementDef& def = game::achievementDef(id);
    if (def.reward == game::AchievementReward::Buff) {
      game::grantBuff(GAME_STATE.player, def.rewardValue);
    } else if (def.reward == game::AchievementReward::Skill) {
      game::grantSkill(GAME_STATE.player, def.rewardValue);
    }
  }

  unlockedThisRun[idx] = true;
  if (unlocked[idx]) return;
  unlocked[idx] = true;
  persistenceDirty_ = true;
  GAME_STATE.addMessage(flavorText);
  if (pendingCount_ >= MAX_PENDING_UNLOCKS) {
    LOG_ERR("ACH", "pending-unlock queue full, dropping notification for: %s", flavorText);
  } else {
    pendingIds_[pendingCount_] = id;
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

game::AchievementId AchievementBus::consumeNewUnlockId() {
  if (pendingCount_ == 0) {
    return game::AchievementId::Count;
  }
  game::AchievementId id = pendingIds_[0];
  // Shift remaining entries down -- pendingCount_ is bounded at
  // MAX_PENDING_UNLOCKS (8), so this is a handful of element copies at most.
  for (uint8_t i = 1; i < pendingCount_; i++) {
    pendingIds_[i - 1] = pendingIds_[i];
  }
  pendingCount_--;
  return id;
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

  // Data-driven pool (IronStomach onward) runs additively alongside the
  // hand-coded switch above -- never replaces it.
  evaluateConditions(event);
}

namespace {

int32_t readCounterField(game::CounterField field, uint32_t lifetimeTilesWalked,
                          uint8_t floorsExploredFully) {
  switch (field) {
    case game::CounterField::Depth: return GAME_STATE.player.dungeonDepth;
    case game::CounterField::Kills: return GAME_STATE.player.kills;
    case game::CounterField::Gold: return GAME_STATE.player.gold;
    case game::CounterField::TurnCount: return static_cast<int32_t>(GAME_STATE.player.turnCount);
    case game::CounterField::Hunger: return GAME_STATE.player.hunger;
    case game::CounterField::Hp: return GAME_STATE.player.hp;
    case game::CounterField::TilesWalked: return static_cast<int32_t>(lifetimeTilesWalked);
    case game::CounterField::FloorsFullyExplored: return floorsExploredFully;
  }
  return 0;
}

int32_t readEventField(game::EventField field, const game::GameEvent& event) {
  switch (field) {
    case game::EventField::Damage: return event.damage;
    case game::EventField::HpAfter: return event.hpAfter;
    case game::EventField::MonsterMaxHp: return event.monsterMaxHp;
    case game::EventField::MonsterMinDepth: return event.monsterMinDepth;
    case game::EventField::NewLevel: return event.newLevel;
  }
  return 0;
}

bool compare(game::CompareOp op, int32_t lhs, int32_t rhs) {
  switch (op) {
    case game::CompareOp::GreaterEqual: return lhs >= rhs;
    case game::CompareOp::LessEqual: return lhs <= rhs;
    case game::CompareOp::Equal: return lhs == rhs;
  }
  return false;
}

}  // namespace

void AchievementBus::evaluateConditions(const game::GameEvent& event) {
  using game::ConditionType;

  for (uint8_t i = 0; i < game::CONDITION_COUNT; i++) {
    const game::AchievementCondition& c = game::CONDITIONS[i];
    if (isUnlocked(c.id)) continue;  // lifetime-unlocked already, nothing left to evaluate

    switch (c.type) {
      case ConditionType::CounterCompare: {
        int32_t lhs = readCounterField(c.field, lifetimeTilesWalked, floorsExploredFully);
        if (compare(c.op, lhs, c.value)) {
          unlock(c.id, game::achievementShortName(c.id));
        }
        break;
      }

      case ConditionType::EventFieldMatch: {
        if (event.type != c.eventType) break;
        int32_t lhs = readEventField(c.eventField, event);
        if (compare(c.op, lhs, c.value)) {
          unlock(c.id, game::achievementShortName(c.id));
        }
        break;
      }

      case ConditionType::EventCount: {
        if (event.type != c.eventType) break;
        conditionEventCounts_[i]++;
        if (conditionEventCounts_[i] >= c.value) {
          unlock(c.id, game::achievementShortName(c.id));
        }
        break;
      }

      case ConditionType::NoEventInWindow: {
        // Any occurrence of the watched event type breaks the window,
        // regardless of which row's window (Run vs Floor) it belongs to --
        // the window-scoped check below decides when to actually fire.
        if (event.type == c.eventType) {
          conditionWindowBroken_[i] = true;
        }
        if (c.window == game::NoEventWindow::Floor && event.type == game::GameEventType::FloorChanged) {
          if (!conditionWindowBroken_[i]) {
            unlock(c.id, game::achievementShortName(c.id));
          }
          conditionWindowBroken_[i] = false;  // new floor, new window
        } else if (c.window == game::NoEventWindow::Run && event.type == game::GameEventType::PlayerDied) {
          if (!conditionWindowBroken_[i]) {
            unlock(c.id, game::achievementShortName(c.id));
          }
        }
        break;
      }

      case ConditionType::ItemSpecific: {
        if (event.type != c.eventType) break;
        if (event.itemType != c.itemType) break;
        conditionEventCounts_[i]++;
        if (conditionEventCounts_[i] >= c.value) {
          unlock(c.id, game::achievementShortName(c.id));
        }
        break;
      }

      case ConditionType::Compound: {
        if (c.compoundA < 0 || c.compoundB < 0) break;
        bool a = isUnlockedThisRun(game::CONDITIONS[c.compoundA].id);
        bool b = isUnlockedThisRun(game::CONDITIONS[c.compoundB].id);
        if (a && b) {
          unlock(c.id, game::achievementShortName(c.id));
        }
        break;
      }
    }
  }
}
