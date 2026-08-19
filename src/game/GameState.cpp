#include "GameState.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstring>

#include "AchievementBus.h"
#include "FlavorText.h"
#include "Pet.h"

namespace {
constexpr uint8_t SAVE_FILE_VERSION = 6;  // v2: turnCount widened to uint32_t, added kills +
                                          // combatRngState to Player (Phase 7, one bump for all three)
                                          // v3: added hunger to Player (Phase 11)
                                          // v4: added activeSponsorId to Player (Phase 11 sponsors)
                                          // v5: added activeBuffIds/activeBuffCount/activeSkillIds/
                                          // activeSkillCount to Player (achievement reward work).
                                          // Migrated, not invalidated -- see PlayerV4/readPlayerField
                                          // below: a v4 file loads its existing fields as-is and gets
                                          // the new buff/skill lists defaulted empty, so an in-progress
                                          // run on a player's device is never silently wiped by this bump.
                                          // v6: added Pet to GameState (Job Phase 3 companion). A v5
                                          // (or migrated v4) file has no pet bytes at all -- loaded with
                                          // pet.active left false, exactly like "no companion yet",
                                          // never invalidated either.

// Mirrors Player's exact v4 on-disk layout (everything up to and including
// activeSponsorId, before activeBuffIds/activeBuffCount/activeSkillIds/
// activeSkillCount were added). Field-by-field copy into a real
// game::Player{} on load rather than a raw reinterpret -- keeps this legacy
// shape decoupled from the live struct so a future version bump can't
// accidentally corrupt both.
struct PlayerV4 {
  int16_t x = 0;
  int16_t y = 0;
  uint16_t hp = 20;
  uint16_t maxHp = 20;
  uint16_t mp = 5;
  uint16_t maxMp = 5;
  uint16_t strength = 10;
  uint16_t dexterity = 10;
  uint16_t constitution = 10;
  uint16_t intelligence = 10;
  uint16_t charLevel = 1;
  uint32_t experience = 0;
  uint16_t gold = 0;
  uint8_t dungeonDepth = 1;
  uint32_t gameSeed = 0;
  uint32_t turnCount = 0;
  uint16_t kills = 0;
  uint32_t combatRngState = 1;
  uint16_t hunger = 0;
  uint8_t activeSponsorId = 0;
};

game::Player playerFromV4(const PlayerV4& v4) {
  game::Player p{};
  p.x = v4.x;
  p.y = v4.y;
  p.hp = v4.hp;
  p.maxHp = v4.maxHp;
  p.mp = v4.mp;
  p.maxMp = v4.maxMp;
  p.strength = v4.strength;
  p.dexterity = v4.dexterity;
  p.constitution = v4.constitution;
  p.intelligence = v4.intelligence;
  p.charLevel = v4.charLevel;
  p.experience = v4.experience;
  p.gold = v4.gold;
  p.dungeonDepth = v4.dungeonDepth;
  p.gameSeed = v4.gameSeed;
  p.turnCount = v4.turnCount;
  p.kills = v4.kills;
  p.combatRngState = v4.combatRngState;
  p.hunger = v4.hunger;
  p.activeSponsorId = v4.activeSponsorId;
  // activeBuffIds/activeBuffCount/activeSkillIds/activeSkillCount stay at
  // their game::Player{} defaults (empty) -- a pre-existing run simply has no
  // achievement-granted buffs/skills yet, which is exactly correct: it never
  // had this reward system before.
  return p;
}
constexpr char SAVE_DIR[] = "/.crosspoint/game";
constexpr char SAVE_FILE[] = "/.crosspoint/game/save.bin";

// A log message longer than this in a save file cannot be one this build ever
// wrote (see addMessage()/game log usage) -- treated as corrupt rather than
// trusted verbatim, so a bogus length field can't drive a huge std::string
// resize.
constexpr uint32_t MAX_MESSAGE_LEN = 512;

// readPod()/readString() discard the actual-bytes-read count everywhere else
// in the codebase, so a truncated file today reads partial/garbage data with
// zero error signal. These checked variants are local to this file (not
// folded into the shared Serialization.h, which other systems like the
// book/EPUB cache also depend on) and are the only source of the "truncated"
// validity reason below.
template <typename T>
bool readPodChecked(HalFile& file, T& value) {
  int n = file.read(reinterpret_cast<uint8_t*>(&value), sizeof(T));
  return n == static_cast<int>(sizeof(T));
}

bool readStringChecked(HalFile& file, std::string& s) {
  uint32_t len;
  if (!readPodChecked(file, len)) return false;
  if (len > MAX_MESSAGE_LEN) return false;
  s.resize(len);
  if (len == 0) return true;
  int n = file.read(reinterpret_cast<uint8_t*>(&s[0]), len);
  return n == static_cast<int>(len);
}

// Plain-data staging area for a parsed save.bin -- mirrors GameState's
// persisted fields but lives independently of any live instance, so a
// stale/incompatible file can be rejected atomically without ever touching
// GAME_STATE's current run.
struct StagedSave {
  game::Player player{};
  game::Pet pet{};
  game::Item inventory[game::MAX_INVENTORY]{};
  uint8_t inventoryCount = 0;
  std::string messages[game::MAX_MESSAGES];
  uint8_t messageCount = 0;
  uint8_t messageHead = 0;
};

// Parses and validates save.bin's contents into a staging area, without
// touching any live GameState. Both GameState::loadFromFile() and
// GameState::validateSaveFile() call this exact function -- there is only one
// definition of "valid" for the whole-game save file. On any Invalid result,
// `reason` is one of "version", "truncated", or "bad index" (never blank),
// and the staged output must not be trusted/committed.
SaveValidity parseSaveFile(HalFile& file, StagedSave& out) {
  SaveValidity result;

  uint8_t version;
  if (!readPodChecked(file, version)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  // v4 is the one prior layout this build still knows how to migrate (see
  // PlayerV4/playerFromV4 above) -- anything older or newer than that is
  // genuinely unsafe to reinterpret and gets rejected, same as before.
  if (version != SAVE_FILE_VERSION && version != 4) {
    // Player is written as raw POD -- any layout change (this file's
    // version history widened turnCount and added fields over time) makes an
    // older file's bytes unsafe to reinterpret as the new struct. Reject
    // rather than risk a corrupted read.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "version";
    return result;
  }

  if (version == 4) {
    PlayerV4 legacy;
    if (!readPodChecked(file, legacy)) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
    out.player = playerFromV4(legacy);
  } else {
    if (!readPodChecked(file, out.player)) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
  }

  // Pet (added v6 -- absent from v4/v5 files, out.pet stays default-constructed
  // i.e. active=false, meaning "no companion yet", same as a fresh run).
  if (version >= 6) {
    if (!readPodChecked(file, out.pet)) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
  }

  // Inventory
  if (!readPodChecked(file, out.inventoryCount)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  if (out.inventoryCount > game::MAX_INVENTORY) {
    // A count field this far out of range means the file wasn't written by a
    // build with this MAX_INVENTORY -- same load-boundary trust issue as an
    // out-of-range monster.type in a level file (GameSave.cpp). Reject rather
    // than clamp: silently dropping inventory items past the clamp point is
    // data loss with no error signal, not a safe recovery.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "bad index";
    return result;
  }
  for (uint8_t i = 0; i < out.inventoryCount; i++) {
    if (!readPodChecked(file, out.inventory[i])) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
  }

  // Message log
  if (!readPodChecked(file, out.messageCount) || !readPodChecked(file, out.messageHead)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  if (out.messageCount > game::MAX_MESSAGES) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "bad index";
    return result;
  }
  for (uint8_t i = 0; i < out.messageCount; i++) {
    int idx = (out.messageHead + i) % game::MAX_MESSAGES;
    if (!readStringChecked(file, out.messages[idx])) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
  }

  result.status = SaveValidity::Status::Valid;
  return result;
}
}  // namespace

GameState GameState::instance;

void GameState::newGame(uint32_t seed) {
  player = game::Player{};
  player.gameSeed = seed;
  player.hp = 20;
  player.maxHp = 20;
  player.mp = 5;
  player.maxMp = 5;
  player.strength = 10;
  player.dexterity = 10;
  player.constitution = 10;
  player.intelligence = 10;
  player.charLevel = 1;
  player.experience = 0;
  player.gold = 0;
  player.dungeonDepth = 1;
  player.turnCount = 0;
  player.kills = 0;
  player.combatRngState = seed ? seed : 1;  // game::Rng treats 0 as invalid; mirror that here

  pet = game::Pet{};
  game::rollNewPet(pet);
  ACHIEVEMENTS.unlock(game::AchievementId::FirstFriend, "Tamed your first companion.");

  inventoryCount = 0;
  memset(inventory, 0, sizeof(inventory));

  messageCount = 0;
  messageHead = 0;
  for (auto& msg : messages) {
    msg.clear();
  }

  addMessage("You enter the World Dungeon...");

  // Fresh run — the death/victory screen's achievement list should only reflect
  // what THIS run earned, not carryover from a prior run in the same session.
  ACHIEVEMENTS.resetRun();
  // Fresh run has no "last shown" flavour-variant history to avoid repeating against.
  FLAVOR_TEXT.resetRun();
}

void GameState::addMessage(const char* msg) {
  if (messageCount < game::MAX_MESSAGES) {
    // Buffer not full yet — append at next slot
    int idx = (messageHead + messageCount) % game::MAX_MESSAGES;
    messages[idx] = msg;
    messageCount++;
  } else {
    // Buffer full — overwrite oldest, advance head
    messages[messageHead] = msg;
    messageHead = (messageHead + 1) % game::MAX_MESSAGES;
  }
}

const std::string& GameState::getMessage(int recencyIndex) const {
  static const std::string empty;
  if (recencyIndex < 0 || recencyIndex >= messageCount) {
    return empty;
  }
  // Most recent is at (head + count - 1), go backwards by recencyIndex
  int idx = (messageHead + messageCount - 1 - recencyIndex) % game::MAX_MESSAGES;
  return messages[idx];
}

uint32_t GameState::rollRange(uint32_t max) {
  game::Rng rng(player.combatRngState);
  uint32_t v = rng.nextRange(max);
  player.combatRngState = rng.state;
  return v;
}

int GameState::rollRangeInclusive(int min, int max) {
  game::Rng rng(player.combatRngState);
  int v = rng.nextRangeInclusive(min, max);
  player.combatRngState = rng.state;
  return v;
}

game::Item GameState::rollLootItem(uint8_t depth) {
  game::Rng rng(player.combatRngState);
  game::Item item = game::rollLootItem(depth, rng);
  player.combatRngState = rng.state;
  return item;
}

bool GameState::saveToFile() const {
  Storage.mkdir(SAVE_DIR);

  HalFile file;
  if (!Storage.openFileForWrite("DM", SAVE_FILE, file)) {
    LOG_ERR("DM", "Failed to open save file for writing");
    return false;
  }

  serialization::writePod(file, SAVE_FILE_VERSION);

  // Player struct (written as raw POD)
  serialization::writePod(file, player);

  // Pet (added v6)
  serialization::writePod(file, pet);

  // Inventory
  serialization::writePod(file, inventoryCount);
  for (uint8_t i = 0; i < inventoryCount; i++) {
    serialization::writePod(file, inventory[i]);
  }

  // Message log
  serialization::writePod(file, messageCount);
  serialization::writePod(file, messageHead);
  for (uint8_t i = 0; i < messageCount; i++) {
    int idx = (messageHead + i) % game::MAX_MESSAGES;
    serialization::writeString(file, messages[idx]);
  }

  file.close();
  LOG_INF("DM", "Game saved (depth %u, turn %u)", player.dungeonDepth, player.turnCount);
  return true;
}

bool GameState::loadFromFile() {
  HalFile file;
  if (!Storage.openFileForRead("DM", SAVE_FILE, file)) {
    return false;
  }

  // Staged into a local struct and only committed to this instance once the
  // ENTIRE file has validated cleanly -- same atomic reject-not-clamp
  // philosophy as GameSave::loadLevel(). The caller's current run state is
  // left completely untouched on any rejection.
  StagedSave staged;
  SaveValidity validity = parseSaveFile(file, staged);
  file.close();

  if (validity.status != SaveValidity::Status::Valid) {
    LOG_ERR("DM", "Save file rejected: %s", validity.reason);
    // Defense in depth: never leave whatever run happened to be sitting in
    // this session-lifetime singleton (GameState.h) looking like a live,
    // playable run after a rejection. GameActivity::onEnter() is expected to
    // route a rejected load through CorruptSaveNotice and call newGame()
    // itself with a real fresh seed before play resumes -- but a caller that
    // (by bug or omission) discards this return value and falls straight
    // through to play must not end up resuming the stale prior run either.
    // Same seed derivation as GameTitleActivity.cpp and
    // GameActivity::resolveWholeRunCorruptNotice() use for a genuine new run
    // -- there is no salvageable seed from a rejected save.bin, and this
    // fallback must be a real, playable run rather than a fixed dungeon.
    newGame(static_cast<uint32_t>(millis()) ^ 0xDEADBEEFu);
    return false;
  }

  player = staged.player;
  pet = staged.pet;
  inventoryCount = staged.inventoryCount;
  memcpy(inventory, staged.inventory, sizeof(inventory));
  messageCount = staged.messageCount;
  messageHead = staged.messageHead;
  for (int i = 0; i < game::MAX_MESSAGES; i++) {
    messages[i] = staged.messages[i];
  }

  LOG_INF("DM", "Game loaded (depth %u, turn %u)", player.dungeonDepth, player.turnCount);
  return true;
}

bool GameState::hasSaveFile() const {
  return Storage.exists(SAVE_FILE);
}

void GameState::deleteSaveFile() const {
  Storage.remove(SAVE_FILE);
  LOG_INF("DM", "Save file deleted");
}

SaveValidity GameState::validateSaveFile() {
  SaveValidity result;
  if (!Storage.exists(SAVE_FILE)) {
    result.status = SaveValidity::Status::NotPresent;
    return result;
  }

  HalFile file;
  if (!Storage.openFileForRead("DM", SAVE_FILE, file)) {
    // Exists but can't be opened -- treat as corrupt rather than silently
    // reporting NotPresent, since Storage.exists() already said yes.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  StagedSave staged;
  result = parseSaveFile(file, staged);
  file.close();
  return result;
}
