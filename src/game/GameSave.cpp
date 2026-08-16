#include "GameSave.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Serialization.h>

#include <cstdio>
#include <cstring>

namespace {
constexpr uint8_t LEVEL_FILE_VERSION = 3;  // v3: added gameSeed header field (cross-seed rejection)
constexpr char SAVE_DIR[] = "/.crosspoint/game";

void levelPath(uint8_t depth, char* buf, size_t bufSize) {
  snprintf(buf, bufSize, "/.crosspoint/game/level_%02u.bin", depth);
}

// readPod()/file.read() discard the actual-bytes-read count everywhere else in
// the codebase, so a truncated file today reads partial/garbage data with zero
// error signal. These checked variants are local to this file (not folded into
// the shared Serialization.h, which other systems like the book/EPUB cache also
// depend on) and are the only source of the "truncated" validity reason below.
template <typename T>
bool readPodChecked(HalFile& file, T& value) {
  int n = file.read(reinterpret_cast<uint8_t*>(&value), sizeof(T));
  return n == static_cast<int>(sizeof(T));
}

bool readBytesChecked(HalFile& file, uint8_t* buf, size_t count) {
  int n = file.read(buf, static_cast<size_t>(count));
  return n == static_cast<int>(count);
}

// Parses and validates a level file's contents into staged buffers, without
// touching any caller-owned live state. Both GameSave::loadLevel() and
// GameSave::validateLevel() call this exact function -- there is only one
// definition of "valid" for a level file. On any Invalid result, `reason` is
// one of "version", "truncated", "seed", or "bad index" (never blank), and
// the staged output must not be trusted/committed.
SaveValidity parseLevelFile(HalFile& file, uint32_t expectedSeed, uint8_t* stagedFog, uint8_t* stagedDoor,
                            game::Monster* stagedMonsters, uint8_t& stagedMonsterCount, game::Item* stagedItems,
                            uint8_t& stagedItemCount, uint8_t& outVersion) {
  SaveValidity result;

  uint8_t version;
  if (!readPodChecked(file, version)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  // Exact match, not "<=": a file from before the gameSeed field existed has
  // no seed to check and must not be grandfathered in -- rejecting it here is
  // the correct outcome (it would otherwise silently overlay state from a
  // dungeon layout generated under an unknown, unverifiable seed).
  if (version != LEVEL_FILE_VERSION) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "version";
    return result;
  }
  outVersion = version;

  uint8_t savedDepth;
  if (!readPodChecked(file, savedDepth)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  uint32_t savedSeed;
  if (!readPodChecked(file, savedSeed)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  if (savedSeed != expectedSeed) {
    // This level file belongs to a different run (e.g. stale leftover from a
    // previous save that was purged/replaced) -- reject rather than overlay
    // its fog/monsters/items onto a dungeon generated from a different seed.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "seed";
    return result;
  }

  if (!readBytesChecked(file, stagedFog, game::FOG_SIZE)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  if (!readBytesChecked(file, stagedDoor, game::FOG_SIZE)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  // Monsters
  if (!readPodChecked(file, stagedMonsterCount)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  if (stagedMonsterCount > game::MAX_MONSTERS) {
    stagedMonsterCount = game::MAX_MONSTERS;
  }
  for (uint8_t i = 0; i < stagedMonsterCount; i++) {
    if (!readPodChecked(file, stagedMonsters[i])) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
    // monster.type is used as a raw, unchecked index into MONSTER_DEFS[] (and
    // the active theme's sprite table, same size) by both render call sites.
    // A stale level file can carry any byte here -- an exact LEVEL_FILE_VERSION
    // match says nothing about whether MONSTER_DEFS[] has grown or shrunk since
    // that file was written, so this is a load-boundary trust issue, not a
    // range-of-valid-gameplay issue.
    // Reject the whole level rather than clamp: a corrupt monster silently
    // becoming monster zero is a wrong monster, not a recovered one, and the
    // freshly generated floor the caller already has is the correct fallback.
    if (stagedMonsters[i].type >= game::MONSTER_DEF_COUNT) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "bad index";
      return result;
    }
  }

  // Items
  if (!readPodChecked(file, stagedItemCount)) {
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }
  if (stagedItemCount > game::MAX_ITEMS_PER_LEVEL) {
    stagedItemCount = game::MAX_ITEMS_PER_LEVEL;
  }
  for (uint8_t i = 0; i < stagedItemCount; i++) {
    if (!readPodChecked(file, stagedItems[i])) {
      result.status = SaveValidity::Status::Invalid;
      result.reason = "truncated";
      return result;
    }
  }

  result.status = SaveValidity::Status::Valid;
  return result;
}
}  // namespace

bool GameSave::saveLevel(uint8_t depth, uint32_t gameSeed, const uint8_t* fogOfWar, const uint8_t* doorOpen,
                         const game::Monster* monsters, uint8_t monsterCount, const game::Item* items,
                         uint8_t itemCount) {
  Storage.mkdir(SAVE_DIR);

  char path[48];
  levelPath(depth, path, sizeof(path));

  HalFile file;
  if (!Storage.openFileForWrite("DM", path, file)) {
    LOG_ERR("DM", "Failed to write level %u", depth);
    return false;
  }

  serialization::writePod(file, LEVEL_FILE_VERSION);
  serialization::writePod(file, depth);
  serialization::writePod(file, gameSeed);

  // Fog of war bitmap
  file.write(fogOfWar, game::FOG_SIZE);

  // Door-open bitmap (same MAP_SIZE-bit layout as fogOfWar)
  file.write(doorOpen, game::FOG_SIZE);

  // Monsters
  serialization::writePod(file, monsterCount);
  for (uint8_t i = 0; i < monsterCount; i++) {
    serialization::writePod(file, monsters[i]);
  }

  // Items
  serialization::writePod(file, itemCount);
  for (uint8_t i = 0; i < itemCount; i++) {
    serialization::writePod(file, items[i]);
  }

  file.close();
  LOG_INF("DM", "Level %u saved (%u monsters, %u items)", depth, monsterCount, itemCount);
  return true;
}

bool GameSave::loadLevel(uint8_t depth, uint32_t expectedSeed, uint8_t* fogOfWar, uint8_t* doorOpen,
                         game::Monster* monsters, uint8_t& monsterCount, game::Item* items, uint8_t& itemCount) {
  char path[48];
  levelPath(depth, path, sizeof(path));

  HalFile file;
  if (!Storage.openFileForRead("DM", path, file)) {
    return false;
  }

  // Everything is staged into local heap buffers and only copied into the
  // caller's output arrays (fogOfWar/doorOpen/monsters/items) once the ENTIRE
  // file has validated cleanly. A stale/incompatible file must be rejectable
  // atomically: the caller has already generated a fresh floor before calling
  // us (GameActivity::loadOrGenerateLevel()), and reading fields straight into
  // its live arrays would let a partially-valid prefix (e.g. fog bytes, or the
  // first few monsters before a bad one) silently overwrite that fresh state
  // even on a load we ultimately reject.
  auto stagedFog = makeUniqueNoThrow<uint8_t[]>(game::FOG_SIZE);
  auto stagedDoor = makeUniqueNoThrow<uint8_t[]>(game::FOG_SIZE);
  auto stagedMonsters = makeUniqueNoThrow<game::Monster[]>(game::MAX_MONSTERS);
  auto stagedItems = makeUniqueNoThrow<game::Item[]>(game::MAX_ITEMS_PER_LEVEL);
  if (!stagedFog || !stagedDoor || !stagedMonsters || !stagedItems) {
    LOG_ERR("DM", "Level %u load failed: OOM staging buffers", depth);
    file.close();
    return false;
  }

  uint8_t stagedMonsterCount = 0;
  uint8_t stagedItemCount = 0;
  uint8_t parsedVersion = 0;
  SaveValidity validity = parseLevelFile(file, expectedSeed, stagedFog.get(), stagedDoor.get(), stagedMonsters.get(),
                                         stagedMonsterCount, stagedItems.get(), stagedItemCount, parsedVersion);
  file.close();

  if (validity.status != SaveValidity::Status::Valid) {
    LOG_ERR("DM", "Level %u save rejected: %s", depth, validity.reason);
    return false;
  }

  // Full file validated -- commit the staged data as the one atomic write to
  // the caller's arrays. A rejection above never reaches this line, so the
  // caller's freshly generated floor is left completely untouched. Every file
  // that reaches here is an exact LEVEL_FILE_VERSION match, so the door-open
  // bitmap is always present and always committed.
  memcpy(fogOfWar, stagedFog.get(), game::FOG_SIZE);
  if (doorOpen != nullptr) {
    memcpy(doorOpen, stagedDoor.get(), game::FOG_SIZE);
  }
  memcpy(monsters, stagedMonsters.get(), stagedMonsterCount * sizeof(game::Monster));
  monsterCount = stagedMonsterCount;
  memcpy(items, stagedItems.get(), stagedItemCount * sizeof(game::Item));
  itemCount = stagedItemCount;

  LOG_INF("DM", "Level %u loaded (%u monsters, %u items)", depth, monsterCount, itemCount);
  return true;
}

SaveValidity GameSave::validateLevel(uint8_t depth, uint32_t expectedSeed) {
  char path[48];
  levelPath(depth, path, sizeof(path));

  SaveValidity result;
  if (!Storage.exists(path)) {
    result.status = SaveValidity::Status::NotPresent;
    return result;
  }

  HalFile file;
  if (!Storage.openFileForRead("DM", path, file)) {
    // Exists but can't be opened -- treat as truncated/corrupt rather than
    // silently reporting NotPresent, since Storage.exists() already said yes.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  auto stagedFog = makeUniqueNoThrow<uint8_t[]>(game::FOG_SIZE);
  auto stagedDoor = makeUniqueNoThrow<uint8_t[]>(game::FOG_SIZE);
  auto stagedMonsters = makeUniqueNoThrow<game::Monster[]>(game::MAX_MONSTERS);
  auto stagedItems = makeUniqueNoThrow<game::Item[]>(game::MAX_ITEMS_PER_LEVEL);
  if (!stagedFog || !stagedDoor || !stagedMonsters || !stagedItems) {
    file.close();
    // OOM staging the exact same buffers loadLevel() needs -- can't validate.
    // Not a statement about the file itself, so don't claim Invalid; NotPresent
    // is also wrong. Treat conservatively as Invalid/truncated so the audit
    // screen surfaces something actionable rather than silently omitting the
    // row.
    result.status = SaveValidity::Status::Invalid;
    result.reason = "truncated";
    return result;
  }

  uint8_t stagedMonsterCount = 0;
  uint8_t stagedItemCount = 0;
  uint8_t parsedVersion = 0;
  result = parseLevelFile(file, expectedSeed, stagedFog.get(), stagedDoor.get(), stagedMonsters.get(),
                          stagedMonsterCount, stagedItems.get(), stagedItemCount, parsedVersion);
  file.close();
  return result;
}

bool GameSave::hasLevel(uint8_t depth) {
  char path[48];
  levelPath(depth, path, sizeof(path));
  return Storage.exists(path);
}

void GameSave::deleteLevel(uint8_t depth) {
  char path[48];
  levelPath(depth, path, sizeof(path));
  Storage.remove(path);
}

void GameSave::deleteAll() {
  // Delete save file
  Storage.remove("/.crosspoint/game/save.bin");

  // Delete all level files
  for (uint8_t i = 1; i <= game::MAX_DEPTH; i++) {
    char path[48];
    levelPath(i, path, sizeof(path));
    Storage.remove(path);
  }

  LOG_INF("DM", "All save data deleted");
}
