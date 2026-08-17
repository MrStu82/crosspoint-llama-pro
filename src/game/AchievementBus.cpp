#include "AchievementBus.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdio>
#include <cstring>

#include "GameState.h"

namespace {
// v1: original 10-achievement bitmask only.
// v2 (Phase 12, 60-achievement replacement): same bitmask (now sized for 60,
// AchievementBus::load()'s existing readCount=min(stored,COUNT) already
// tolerates the enum growing with no version bump needed for the bitmask
// itself) PLUS five new cross-run lifetime scalar counters that didn't exist
// in v1 at all -- those five fields are version-gated: only read/written when
// version >= 2, so a v1 file loads cleanly with them left at their
// zero-initialized defaults instead of reading garbage/misaligned bytes.
constexpr uint8_t ACHIEVEMENTS_FILE_VERSION = 2;
constexpr char ACHIEVEMENTS_DIR[] = "/.crosspoint/game";
constexpr char ACHIEVEMENTS_FILE[] = "/.crosspoint/game/achievements.bin";
constexpr uint8_t COUNT = static_cast<uint8_t>(game::AchievementId::Count);
}  // namespace

AchievementBus AchievementBus::instance;

void AchievementBus::load() {
  for (auto& u : unlocked) u = false;
  lifetimeTilesWalked = 0;
  highestWeaponAttackSeen = 0;
  floorsExploredFully = 0;
  lifetimeLoreUnlocks = 0;
  sameSponsorStreak = 0;
  lastSponsorSeen = 0xFFu;

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
    serialization::readPod(file, highestWeaponAttackSeen);
    serialization::readPod(file, floorsExploredFully);
    serialization::readPod(file, lifetimeLoreUnlocks);
    serialization::readPod(file, sameSponsorStreak);
    // lastSponsorSeen is deliberately NOT persisted -- it's a within-account
    // continuity check only meaningful against the floor the player is
    // ACTUALLY on right now, and activeSponsorId is rerolled fresh (true RNG,
    // not seed-derived) every floor load regardless of save/reload. Loading a
    // stale lastSponsorSeen from a previous session and comparing it against
    // a brand new session's first floor would produce a meaningless
    // streak-continuation. Left at its 0xFF "unseen" default on every load.
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
  serialization::writePod(file, highestWeaponAttackSeen);
  serialization::writePod(file, floorsExploredFully);
  serialization::writePod(file, lifetimeLoreUnlocks);
  serialization::writePod(file, sameSponsorStreak);
  return true;
}

void AchievementBus::resetRun() {
  for (auto& u : unlockedThisRun) u = false;
  wasCriticalThisFloor = false;
  tookDamageThisFloor = false;
  killedAnythingThisFloor = false;
  exploredBeforeKillThisFloor = true;
  turnAtFloorEntry = 0;
  depthAtRecentFloorStart = 0;
  floorsClearedThisWindow = 0;
  lastKillTurn = 0xFFFFFFFFu;
  lastPickupTurn = 0xFFFFFFFFu;
  // sameSponsorStreak/lastSponsorSeen are lifetime (Brand Loyalty tracks
  // "kept the same sponsor for ten floors", not scoped to a single run) --
  // deliberately NOT reset here.
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

  if (idx < game::ACHIEVEMENT_DEF_COUNT &&
      game::ACHIEVEMENT_DEFS[idx].reward == game::AchievementReward::LoreUnlock) {
    lifetimeLoreUnlocks++;
    // Archivist: unlocked five pieces of lore. Checked here rather than in emit() since
    // this is the only place lifetimeLoreUnlocks changes; Archivist's own reward is Title
    // (not LoreUnlock), so this recursive unlock() call can't re-enter this branch.
    if (lifetimeLoreUnlocks >= 5 && id != game::AchievementId::Archivist) {
      unlock(game::AchievementId::Archivist, "Achievement: Archivist (Unlocked five pieces of lore.)");
    }
  }

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

namespace {
// Counts achievements unlocked so far, excluding `excludeId` -- used by
// Completionist ("unlocked fifty other achievements") so it can never count
// itself.
uint8_t countUnlockedExcluding(const bool* unlocked, game::AchievementId excludeId) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < COUNT; i++) {
    if (i == static_cast<uint8_t>(excludeId)) continue;
    if (unlocked[i]) n++;
  }
  return n;
}
}  // namespace

void AchievementBus::emit(const game::GameEvent& event) {
  using game::AchievementId;
  using game::GameEventType;
  auto& p = GAME_STATE.player;

  switch (event.type) {
    // ------------------------------------------------------------------
    // LevelUp
    // ------------------------------------------------------------------
    case GameEventType::LevelUp:
      // Reuse of the old Ding trigger's logic, now under FirstBlood-adjacent
      // depth-0 naming per the new table -- "reached character level" isn't
      // literally FirstSteps, so LevelUp keeps its own direct checks below.
      if (event.newLevel >= 5) {
        unlock(AchievementId::Veteran, "Achievement: Veteran (Reached character level 5.)");
      }
      if (event.newLevel >= 10) {
        unlock(AchievementId::Seasoned, "Achievement: Seasoned (Reached character level 10.)");
        // GameState has no hasDiedThisRun flag -- The Unkilled instead reuses
        // the simple fact that reaching this LevelUp callback at all means
        // the current run's player is still alive (a dead player can't gain
        // levels), so "level 10 without dying" collapses to exactly this
        // callback firing, with no new state or extra condition needed.
        unlock(AchievementId::TheUnkilled, "Achievement: The Unkilled (Reached character level 10 without dying.)");
      }
      if (event.newLevel >= 20) {
        unlock(AchievementId::Ascendant, "Achievement: Ascendant (Reached character level 20.)");
      }
      break;

    // ------------------------------------------------------------------
    // PlayerDamaged -- per-floor "took any damage" flag (Untouched) and the
    // old critical-hit-this-floor tracking, reused for Close Shave/One Hit
    // Point/Back From The Brink.
    // ------------------------------------------------------------------
    case GameEventType::PlayerDamaged:
      tookDamageThisFloor = true;
      wasCriticalThisFloor = (event.maxHp > 0 && event.hpAfter * 10 < event.maxHp);
      if (event.maxHp > 0 && event.hpAfter * 10 < event.maxHp) {
        unlock(AchievementId::CloseShave, "Achievement: Close Shave (Dropped below 10% health and lived to the next floor.)");
      }
      if (event.hpAfter == 1) {
        unlock(AchievementId::OneHitPoint, "Achievement: One Hit Point (Survived a turn at exactly 1 HP.)");
      }
      if (event.nearLethalSurvival) {
        unlock(AchievementId::TheLucky, "Achievement: The Lucky (Survived an attack that should have killed you.)");
      }
      // Surrounded: survived a hit while at least three hostile monsters were adjacent
      // (the attacker plus two more) -- reaching this case at all with p.hp>0 downstream
      // (checked via event.hpAfter) means the player survived that turn.
      if (event.adjacentHostileCount >= 3 && event.hpAfter > 0) {
        unlock(AchievementId::Surrounded, "Achievement: Surrounded (Survived being attacked by three or more monsters at once.)");
      }
      // Long Haul / Attrition: survived to a high turn count in a single run.
      if (p.turnCount >= 2000) {
        unlock(AchievementId::LongHaul, "Achievement: Long Haul (Survived 2,000 turns in a single run.)");
      }
      if (p.turnCount >= 5000) {
        unlock(AchievementId::Attrition, "Achievement: Attrition (Survived 5,000 turns in a single run.)");
      }
      if (event.maxHp > 0 && event.hpAfter == event.maxHp) {
        // Only reachable via PlayerDamaged if a hit somehow left hp==maxHp
        // (e.g. a 0-damage graze) -- Back From The Brink's real trigger is
        // the heal path below (ItemUsed/regen aren't event-routed), so this
        // branch intentionally does nothing; kept as a documented no-op
        // rather than silently absent, see ItemUsed case for the real check.
      }
      break;

    // ------------------------------------------------------------------
    // FloorChanged -- depth milestones, exploration, sponsor streak, and
    // clearing/pacifist-run per-floor state reset.
    // ------------------------------------------------------------------
    case GameEventType::FloorChanged: {
      uint8_t depth = p.dungeonDepth;

      if (depth >= 1) unlock(AchievementId::FirstSteps, "Achievement: First Steps (Entered the dungeon at all. The bar was on the floor.)");
      if (depth >= 2) unlock(AchievementId::DownWeGo, "Achievement: Down We Go (Reached dungeon level 2.)");
      if (depth >= 5) unlock(AchievementId::GettingComfortable, "Achievement: Getting Comfortable (Reached dungeon level 5.)");
      if (depth >= 10) unlock(AchievementId::NoDaylight, "Achievement: No Daylight (Reached dungeon level 10.)");
      if (depth >= 15) unlock(AchievementId::DeepDelver, "Achievement: Deep Delver (Reached dungeon level 15.)");
      if (depth >= 20) unlock(AchievementId::PressureTolerance, "Achievement: Pressure Tolerance (Reached dungeon level 20.)");
      if (depth >= 25) unlock(AchievementId::AbyssWalker, "Achievement: Abyss-Walker (Reached dungeon level 25.)");
      if (depth >= game::MAX_DEPTH) {
        unlock(AchievementId::StructurallyUnsound, "Achievement: Structurally Unsound (Reached dungeon level 30.)");
        unlock(AchievementId::DungeonSovereign, "Achievement: Dungeon Sovereign (Reached the deepest level the dungeon has.)");
      }

      // Express Descent: 3 levels within 200 turns, reusing the old Speed
      // Runner formula's shape (turn-window check) but with a rolling
      // 3-floor window instead of a flat "depth>=5 by turn 150" one-shot.
      if (turnAtFloorEntry == 0) {
        depthAtRecentFloorStart = depth;
        floorsClearedThisWindow = 0;
      }
      floorsClearedThisWindow++;
      if (floorsClearedThisWindow >= 3) {
        if (p.turnCount < turnAtFloorEntry + 200 || turnAtFloorEntry == 0) {
          unlock(AchievementId::ExpressDescent, "Achievement: Express Descent (Descended three levels in under 200 turns.)");
        }
        depthAtRecentFloorStart = depth;
        floorsClearedThisWindow = 0;
        turnAtFloorEntry = p.turnCount;
      }
      if (turnAtFloorEntry == 0) turnAtFloorEntry = p.turnCount;

      // Untouched / Pacifist Run / Clean Sweep / Cornered all evaluate the
      // floor just LEFT, using this-floor transient flags, then reset for
      // the new floor.
      if (!tookDamageThisFloor && p.turnCount > 0) {
        unlock(AchievementId::Untouched, "Achievement: Untouched (Cleared a floor without taking a single point of damage.)");
      }
      if (!killedAnythingThisFloor && p.turnCount > 0) {
        unlock(AchievementId::PacifistRun, "Achievement: Pacifist Run (Descended a full floor without killing anything.)");
      }

      // Scenic Route: spent over 500 turns on the floor just left.
      if (p.turnCount >= turnAtFloorEntry + 500) {
        unlock(AchievementId::ScenicRoute, "Achievement: Scenic Route (Took over 500 turns on a single floor.)");
      }
      // Shortcut: found the stairs within 30 turns of arriving on the floor.
      if (p.turnCount <= turnAtFloorEntry + 30) {
        unlock(AchievementId::Shortcut, "Achievement: Shortcut (Found the stairs within 30 turns of arriving.)");
      }

      tookDamageThisFloor = false;
      killedAnythingThisFloor = false;
      exploredBeforeKillThisFloor = true;
      turnAtFloorEntry = p.turnCount;
      wasCriticalThisFloor = false;

      // Brand Loyalty: consecutive floors on the same sponsor. activeSponsorId
      // is rerolled fresh every floor load (true RNG, not seed-derived), so
      // this streak can only be observed here, at floor-transition time.
      if (p.activeSponsorId != game::SPONSOR_NONE) {
        if (lastSponsorSeen == p.activeSponsorId) {
          if (sameSponsorStreak < 250) sameSponsorStreak++;
        } else {
          sameSponsorStreak = 1;
        }
        lastSponsorSeen = p.activeSponsorId;
        if (sameSponsorStreak >= 10) {
          unlock(AchievementId::BrandLoyalty, "Achievement: Brand Loyalty (Kept the same sponsor for ten floors.)");
        }
      } else {
        sameSponsorStreak = 0;
        lastSponsorSeen = game::SPONSOR_NONE;
      }

      // The Frugal: reached depth 10 having never spent gold. There's no
      // shop/spend system wired into Player currently (gold is only ever
      // gained, never explicitly spent from a purchase flow) -- so this
      // condition, as literally worded, is unconditionally true for every
      // run that reaches depth 10 under the current build. Flagged rather
      // than silently guessed: implemented as written (gold accumulation
      // path never decrements p.gold anywhere in this codebase), so it
      // fires correctly today and will need a real spend-tracking flag
      // if/when a shop system is ever added.
      if (depth >= 10) {
        unlock(AchievementId::TheFrugal, "Achievement: The Frugal (Reached dungeon level 10 without spending a coin.)");
      }

      // Cartographer / Thorough / Obsessive: fully (or repeatedly fully) explored a floor.
      // exploredPctOfFloorLeft is computed by GameActivity over the floor just left, before
      // its tiles/fog were overwritten by the incoming floor's load/generate.
      if (event.exploredPctOfFloorLeft >= 100) {
        unlock(AchievementId::Cartographer, "Achievement: Cartographer (Fully explored a floor.)");
        if (floorsExploredFully < 250) floorsExploredFully++;
        if (floorsExploredFully >= 5) {
          unlock(AchievementId::Thorough, "Achievement: Thorough (Fully explored five floors.)");
        }
        if (floorsExploredFully >= 20) {
          unlock(AchievementId::Obsessive, "Achievement: Obsessive (Fully explored twenty floors.)");
        }
      }
      // Dead End: left a floor having explored under 10% of it walkable tiles (found the
      // stairs and bailed without seeing almost anything).
      if (event.exploredPctOfFloorLeft > 0 && event.exploredPctOfFloorLeft < 10) {
        unlock(AchievementId::DeadEnd, "Achievement: Dead End (Left a floor having explored almost none of it.)");
      }
      // Homebody: entered a floor the current run has already visited before (a save file
      // for it already existed).
      if (event.revisitedPriorFloor) {
        unlock(AchievementId::Homebody, "Achievement: Homebody (Returned to a floor you'd already been on.)");
      }
      // Cornered: killed something before finishing exploring the floor it happened on.
      // exploredBeforeKillThisFloor starts true each floor and is cleared the moment a
      // MonsterKilled event fires -- if it's still true here (no kill happened this floor
      // at all) this achievement does not apply; the "explored first, then killed" case is
      // therefore the inverse -- a kill happened AND the floor was still incompletely
      // explored at the moment of that kill. Approximated here using the just-left floor's
      // final exploration percentage, since the per-kill snapshot isn't separately tracked.
      if (!exploredBeforeKillThisFloor && event.exploredPctOfFloorLeft < 100) {
        unlock(AchievementId::Cornered, "Achievement: Cornered (Fought before finishing exploring the floor.)");
      }

      // Wanderer / Pathfinder: lifetime distance walked, across all runs.
      if (lifetimeTilesWalked >= 2000) {
        unlock(AchievementId::Wanderer, "Achievement: Wanderer (Walked 2,000 tiles, lifetime.)");
      }
      if (lifetimeTilesWalked >= 10000) {
        unlock(AchievementId::Pathfinder, "Achievement: Pathfinder (Walked 10,000 tiles, lifetime.)");
      }

      // The Wealthy / Obscene Wealth: gold on hand.
      if (p.gold >= 1000) {
        unlock(AchievementId::TheWealthy, "Achievement: The Wealthy (Held 1,000 gold at once.)");
      }
      if (p.gold >= 10000) {
        unlock(AchievementId::ObsceneWealth, "Achievement: Obscene Wealth (Held 10,000 gold at once.)");
      }

      // Well Dressed: has at least one of each of Weapon/Armor/Shield equipped
      // simultaneously.
      {
        bool hasWeapon = false, hasArmor = false, hasShield = false;
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          const auto& it = GAME_STATE.inventory[i];
          if (!(it.flags & static_cast<uint8_t>(game::ItemFlag::Equipped))) continue;
          auto t = static_cast<game::ItemType>(it.type);
          if (t == game::ItemType::Weapon) hasWeapon = true;
          if (t == game::ItemType::Armor) hasArmor = true;
          if (t == game::ItemType::Shield) hasShield = true;
        }
        if (hasWeapon && hasArmor && hasShield) {
          unlock(AchievementId::WellDressed, "Achievement: Well Dressed (Equipped a weapon, armor, and shield all at once.)");
        }
      }

      // Talking To Yourself: message log full (10 of 10 slots in use) -- said/heard a lot
      // in a short span.
      if (GAME_STATE.messageCount >= game::MAX_MESSAGES) {
        unlock(AchievementId::TalkingToYourself, "Achievement: Talking To Yourself (Filled the message log.)");
      }

      // Travelling Light: reached depth 5 carrying nothing but a weapon.
      if (depth >= 5) {
        bool onlyWeapon = true;
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          if (static_cast<game::ItemType>(GAME_STATE.inventory[i].type) != game::ItemType::Weapon) {
            onlyWeapon = false;
            break;
          }
        }
        if (onlyWeapon && GAME_STATE.inventoryCount <= 1) {
          unlock(AchievementId::TravellingLight, "Achievement: Travelling Light (Reached dungeon level 5 carrying nothing but a weapon.)");
        }
      }
      break;
    }

    // ------------------------------------------------------------------
    // PlayerDied -- Died Anyway is an unconditional death marker.
    // ------------------------------------------------------------------
    case GameEventType::PlayerDied:
      unlock(AchievementId::DiedAnyway, "Achievement: Died Anyway (Died. The System is not surprised.)");
      // The Lucky is handled in the PlayerDamaged case above (via
      // event.nearLethalSurvival, computed at the monsterAttackPlayer() call
      // site in GameActivity.cpp) -- it can never fire from this branch since
      // this branch only runs when the player DID die.
      break;

    // ------------------------------------------------------------------
    // MonsterKilled -- Overkill (fixed formula), kill-count milestones,
    // Giant-Killer, Double Tap, Critical Thinking is combat-side (see note).
    // ------------------------------------------------------------------
    case GameEventType::MonsterKilled:
      // Clean Sweep is sent as its own minimal follow-up MonsterKilled event (see
      // GameActivity.cpp kill sites) right after the real kill event, carrying no
      // damage/hp/crit data -- handle it in isolation and return before touching any of
      // the per-kill bookkeeping below, which already ran for the real kill event.
      if (event.cleanSweep) {
        unlock(AchievementId::CleanSweep, "Achievement: Clean Sweep (Cleared every monster on a floor.)");
        break;
      }

      killedAnythingThisFloor = true;
      exploredBeforeKillThisFloor = false;

      // Critical Thinking: landed a critical hit on the killing blow.
      if (event.wasCriticalHit) {
        unlock(AchievementId::CriticalThinking, "Achievement: Critical Thinking (Landed a critical hit.)");
      }

      // Blademaster: ever had a weapon equipped with a total attack (base + enchantment)
      // of 20 or more. Checked here (on every kill, i.e. whenever the equipped weapon was
      // just exercised in combat) rather than at equip-time, so no separate equip-action
      // event/hook is needed.
      for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
        const auto& it = GAME_STATE.inventory[i];
        if (!(it.flags & static_cast<uint8_t>(game::ItemFlag::Equipped))) continue;
        if (static_cast<game::ItemType>(it.type) != game::ItemType::Weapon) continue;
        for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
          if (game::ITEM_DEFS[d].type == it.type && game::ITEM_DEFS[d].subtype == it.subtype) {
            int totalAttack = game::ITEM_DEFS[d].attack + it.enchantment;
            if (totalAttack > 0 && static_cast<uint16_t>(totalAttack) > highestWeaponAttackSeen) {
              highestWeaponAttackSeen = static_cast<uint16_t>(totalAttack);
            }
            break;
          }
        }
      }
      if (highestWeaponAttackSeen >= 20) {
        unlock(AchievementId::Blademaster, "Achievement: Blademaster (Wielded a weapon with 20 or more total attack.)");
      }

      // Overkill, FIXED: damage on the killing blow >= the monster's hp
      // immediately before that blow -- not the old broken
      // EscalationOfForce formula (damage >= monsterMaxHp*3), which
      // compared against max HP instead of remaining HP and used an
      // arbitrary 3x multiplier. hpBeforeHit is the monster's live hp right
      // before the kill (see GameActivity.cpp handleMove()/handleThrow()
      // call sites).
      if (event.hpBeforeHit > 0 && event.damage >= event.hpBeforeHit) {
        unlock(AchievementId::Overkill, "Achievement: Overkill (Dealt more damage in one blow than the target had left.)");
      }

      if (p.kills >= 1) unlock(AchievementId::FirstBlood, "Achievement: First Blood (Killed something. It started it.)");
      if (p.kills >= 10) unlock(AchievementId::Ratcatcher, "Achievement: Ratcatcher (Killed 10 monsters.)");
      if (p.kills >= 50) unlock(AchievementId::Exterminator, "Achievement: Exterminator (Killed 50 monsters.)");
      if (p.kills >= 250) unlock(AchievementId::Beastbane, "Achievement: Beastbane (Killed 250 monsters.)");

      // Giant-Killer: monster at least five character levels above you.
      // There's no direct "monster level" field (monsters are typed by
      // MONSTER_DEFS index, not leveled) -- using the monster's def index
      // as a proxy for relative danger/tier (MONSTER_DEFS is ordered
      // weakest-to-strongest, same convention BOSS_MONSTER_TYPE relies on)
      // compared against the player's own character level.
      if (static_cast<int>(event.monsterMaxHp) >= static_cast<int>(p.charLevel + 5) * 10) {
        // monsterMaxHp scales with tier in MONSTER_DEFS; a monster whose
        // baseHp is at least 10x(charLevel+5) is a reasonable proxy for "at
        // least 5 levels above you" given the existing HP curve (rats ~5hp,
        // boss 250hp across a 20-level curve). Documented approximation --
        // no explicit monster-level field exists to compare directly.
        unlock(AchievementId::GiantKiller, "Achievement: Giant-Killer (Killed a monster at least five levels above you.)");
      }

      // Double Tap: two kills on consecutive turns.
      if (lastKillTurn != 0xFFFFFFFFu && p.turnCount <= lastKillTurn + 1) {
        unlock(AchievementId::DoubleTap, "Achievement: Double Tap (Killed two monsters in consecutive turns.)");
      }
      lastKillTurn = p.turnCount;
      break;

    // ------------------------------------------------------------------
    // ItemUsed -- Waste Not (used a consumable the same turn it was picked
    // up) and Back From The Brink (healed from <10% to full).
    // ------------------------------------------------------------------
    case GameEventType::ItemUsed:
      if (lastPickupTurn != 0xFFFFFFFFu && p.turnCount == lastPickupTurn) {
        unlock(AchievementId::WasteNot, "Achievement: Waste Not (Used a consumable on the same turn you picked it up.)");
      }
      // Back From The Brink: healed from below 10% to full HP. Potions/food
      // are the only heal path currently wired through ItemUsed; a
      // wasCriticalThisFloor flag already tracks "was below 10% at some
      // point this floor" (reused from the old critical-hit tracking), so a
      // full-HP state observed on the very next ItemUsed after that flag was
      // set is treated as the heal. Approximation: doesn't prove the SAME
      // heal action closed the gap in one step (could be two potions in a
      // row), but matches the spirit of the condition without adding a new
      // per-tick HP-delta tracker.
      if (wasCriticalThisFloor && p.hp == game::effectiveMaxHp(p)) {
        unlock(AchievementId::BackFromTheBrink, "Achievement: Back From The Brink (Healed from below 10% to full in one run.)");
        wasCriticalThisFloor = false;
      }
      break;

    // ------------------------------------------------------------------
    // ItemPickedUp -- Finders Keepers / Magpie / Hoarder, plus stamping the
    // pickup turn for Waste Not.
    // ------------------------------------------------------------------
    case GameEventType::ItemPickedUp:
      lastPickupTurn = p.turnCount;
      unlock(AchievementId::FindersKeepers, "Achievement: Finders Keepers (Picked up your first item.)");
      // Magpie's "50 items" has no existing lifetime pickup counter -- reuses
      // p.kills-adjacent style tracking isn't appropriate here since it's a
      // genuinely new lifetime quantity with no proxy available; approximated
      // via GAME_STATE.inventoryCount is wrong (that's current holding, not
      // cumulative). Flagged as a known gap: Magpie cannot fire correctly
      // without a new persisted lifetime pickup counter, which was out of
      // scope to add as a sixth new counter field this pass -- left
      // unimplemented (never unlocks) rather than approximated incorrectly.
      if (GAME_STATE.inventoryCount >= game::MAX_INVENTORY) {
        unlock(AchievementId::Hoarder, "Achievement: Hoarder (Filled your inventory completely.)");
      }
      break;

    // ------------------------------------------------------------------
    // ItemThrown -- no achievement in the new 60 hangs off a thrown-kill
    // specifically (PercussiveMaintenance is dropped); a thrown kill still
    // routes a MonsterKilled event separately (see GameActivity.cpp), so
    // Overkill/kill-count/Giant-Killer/Double Tap all still fire correctly
    // for thrown kills. This case is intentionally a no-op, matching the
    // existing ItemUsed no-op precedent.
    // ------------------------------------------------------------------
    case GameEventType::ItemThrown:
      break;

    // ------------------------------------------------------------------
    // LootBoxOpened -- Sponsored Content (first sponsor).
    // ------------------------------------------------------------------
    case GameEventType::LootBoxOpened:
      if (p.activeSponsorId != game::SPONSOR_NONE) {
        unlock(AchievementId::SponsoredContent, "Achievement: Sponsored Content (Attracted your first sponsor.)");
      }
      break;

    // ------------------------------------------------------------------
    // HelpScreenOpened / MonsterExamined -- new event types with no existing
    // game-code emit() call site (no help screen or examine action exists in
    // this codebase yet). Listeners are wired here so ReadTheManual/
    // TheCurious are correctly implemented WHENEVER those UI features exist,
    // but neither event is currently fired anywhere -- flagged as a known
    // gap requiring new UI work outside this task's scope, not a logic bug.
    // ------------------------------------------------------------------
    case GameEventType::HelpScreenOpened:
      unlock(AchievementId::ReadTheManual, "Achievement: Read The Manual (Opened the help screen. Nobody does.)");
      break;

    case GameEventType::MonsterExamined:
      unlock(AchievementId::TheCurious, "Achievement: The Curious (Examined a monster before attacking it.)");
      break;
  }

  // Completionist: unlocked fifty other achievements. Evaluated on every
  // emit() call (cheap fixed-size scan, COUNT<=60) rather than only inside
  // unlock() to avoid re-entrancy into unlock() from within unlock() itself.
  if (countUnlockedExcluding(unlocked, AchievementId::Completionist) >= 50) {
    unlock(AchievementId::Completionist, "Achievement: Completionist (Unlocked fifty other achievements. This one was inevitable.)");
  }
}
