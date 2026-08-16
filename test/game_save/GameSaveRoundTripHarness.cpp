// Standalone host harness (no gtest dependency -- compiled directly with g++)
// for Phase 7 reqs 5 and 6 (World Dungeon: Correctness). Per parent's explicit
// correction (2026-08-15): code review is not verification for a save-format
// requirement -- this harness performs genuine write, close, reopen, read
// round trips against real host filesystem I/O (via the extended HalStorage
// stub in test/game_save/stubs/, itself a copy of the pattern already proven
// in /workspace/agent/ach_test/mocks/HalStorage.h for the achievement bus).
//
// This compiles the REAL src/game/GameState.cpp, src/game/GameSave.cpp and
// src/game/AchievementBus.cpp unmodified -- only HalStorage.h/Logging.h are
// swapped for host stubs via the include path, exactly the technique
// ach_test/ already used.
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra \
//        -I test/game_save/stubs -I src/game -I lib/Serialization \
//        test/game_save/GameSaveRoundTripHarness.cpp \
//        src/game/GameState.cpp src/game/GameSave.cpp src/game/AchievementBus.cpp \
//        -o /tmp/game_save_harness
// Run:   /tmp/game_save_harness

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "GameSave.h"
#include "GameState.h"
#include "GameTypes.h"

namespace {

int failures = 0;

#define CHECK(cond, ...)                          \
  do {                                             \
    if (!(cond)) {                                 \
      std::fprintf(stderr, "FAIL: " __VA_ARGS__);  \
      std::fprintf(stderr, "\n");                  \
      failures++;                                  \
    }                                              \
  } while (0)

void resetSdRoot(const char* path) {
  // Best-effort recursive wipe so each test starts from a clean SD image --
  // avoids one test's leftover file masking another's compat-path check.
  std::string cmd = "rm -rf '" + std::string(path) + "'";
  system(cmd.c_str());
  ::mkdir(path, 0755);
}

// --- Req 5: persistent combat RNG stream survives save/reload -------------
//
// Proves the stream is genuinely CONTINUED across a save/reload, not
// restarted from the seed and not reseeded from wall-clock/whatever. Method:
// roll N times, save, corrupt the live state, reload, roll M more times, and
// check those M rolls match rolls [N, N+M) of an INDEPENDENT reference
// game::Rng started fresh from the same seed. If reload silently reset the
// stream back to the seed, roll N+1's value would equal the reference
// stream's roll 1, not its roll N+1 -- a divergence this catches directly.
void testCombatRngSurvivesSaveReload() {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kRollsBeforeSave = 137;
  constexpr int kRollsAfterReload = 89;

  GAME_STATE.newGame(kSeed);

  std::vector<uint32_t> preSaveRolls;
  for (int i = 0; i < kRollsBeforeSave; i++) {
    preSaveRolls.push_back(GAME_STATE.rollRange(1000000));
  }

  CHECK(GAME_STATE.saveToFile(), "saveToFile() returned false");

  // Corrupt the live in-memory state so a pass can only happen via a genuine
  // disk read, not by accident of the singleton still holding the right value.
  GAME_STATE.player.combatRngState = 0xDEADBEEF;
  GAME_STATE.player.hp = 1;
  GAME_STATE.player.turnCount = 999999;

  CHECK(GAME_STATE.loadFromFile(), "loadFromFile() returned false");

  CHECK(GAME_STATE.player.hp == 20, "hp not restored: got %u, expected 20 (newGame default)",
        GAME_STATE.player.hp);
  CHECK(GAME_STATE.player.turnCount == 0, "turnCount not restored: got %u, expected 0",
        GAME_STATE.player.turnCount);

  std::vector<uint32_t> postReloadRolls;
  for (int i = 0; i < kRollsAfterReload; i++) {
    postReloadRolls.push_back(GAME_STATE.rollRange(1000000));
  }

  // Independent reference stream, replayed from scratch, never touching
  // GameState/GameSave at all.
  game::Rng reference(kSeed ? kSeed : 1);
  for (int i = 0; i < kRollsBeforeSave; i++) {
    reference.nextRange(1000000);  // consume, discard -- these are the pre-save rolls
  }
  int mismatches = 0;
  for (int i = 0; i < kRollsAfterReload; i++) {
    uint32_t expected = reference.nextRange(1000000);
    if (expected != postReloadRolls[i]) {
      if (mismatches < 5) {
        std::fprintf(stderr, "  roll %d mismatch: got %u, expected %u\n", i, postReloadRolls[i], expected);
      }
      mismatches++;
    }
  }
  CHECK(mismatches == 0, "%d of %d post-reload rolls diverged from the continued reference stream",
        mismatches, kRollsAfterReload);

  std::printf("[req5] combatRngState: %d pre-save rolls + %d post-reload rolls, %d mismatches vs continued reference stream\n",
              kRollsBeforeSave, kRollsAfterReload, mismatches);
}

// --- Req 6a: door-open bitmap round-trips through a fresh v2 save ---------

void fillPattern(uint8_t* buf, int size, uint8_t seed) {
  for (int i = 0; i < size; i++) buf[i] = static_cast<uint8_t>((i * 37 + seed) & 0xFF);
}

void testDoorOpenBitmapRoundTrips() {
  constexpr uint8_t kDepth = 7;

  std::vector<uint8_t> fog(game::FOG_SIZE), doorOpen(game::FOG_SIZE);
  fillPattern(fog.data(), game::FOG_SIZE, 0x11);
  fillPattern(doorOpen.data(), game::FOG_SIZE, 0x77);

  game::Monster monsters[2];
  monsters[0] = game::Monster{5, 6, 3, 12, static_cast<uint8_t>(game::MonsterState::Hostile)};
  monsters[1] = game::Monster{10, 11, 7, 30, static_cast<uint8_t>(game::MonsterState::Asleep)};

  game::Item items[1];
  items[0] = game::Item{2, 3, static_cast<uint8_t>(game::ItemType::Weapon), 1, 1, 0, 0};

  CHECK(GameSave::saveLevel(kDepth, fog.data(), doorOpen.data(), monsters, 2, items, 1),
        "saveLevel() returned false");

  std::vector<uint8_t> fog2(game::FOG_SIZE, 0xAA), doorOpen2(game::FOG_SIZE, 0xAA);
  game::Monster monsters2[game::MAX_MONSTERS];
  game::Item items2[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount2 = 0, itemCount2 = 0;

  CHECK(GameSave::loadLevel(kDepth, fog2.data(), doorOpen2.data(), monsters2, monsterCount2, items2, itemCount2),
        "loadLevel() returned false");

  CHECK(std::memcmp(fog.data(), fog2.data(), game::FOG_SIZE) == 0, "fogOfWar bitmap did not round-trip intact");
  CHECK(std::memcmp(doorOpen.data(), doorOpen2.data(), game::FOG_SIZE) == 0,
        "doorOpen bitmap did not round-trip intact (req 6 core claim)");
  CHECK(monsterCount2 == 2 && monsters2[0].hp == 12 && monsters2[1].hp == 30, "monster data corrupted by round trip");
  CHECK(itemCount2 == 1 && items2[0].type == static_cast<uint8_t>(game::ItemType::Weapon),
        "item data corrupted by round trip");

  std::printf("[req6a] doorOpen bitmap (%d bytes) round-tripped intact through saveLevel/loadLevel, monsters/items intact\n",
              game::FOG_SIZE);
}

// --- Req 6b: pre-door-persistence (v1) save file loads without crash, ------
// --- doorOpen left untouched when the caller passes nullptr or a buffer ---
//
// Hand-crafts a v1-format file byte-for-byte (the format GameSave.cpp used
// before LEVEL_FILE_VERSION was bumped to 2 for the door-open bitmap) --
// version=1, depth, fog, [no door bytes], monsterCount+monsters,
// itemCount+items -- then loads it through the REAL, current loadLevel() to
// prove the version-gated skip-read actually lines the parser back up on the
// monster/item section instead of reading door bytes that were never written.
void writeV1LevelFile(const char* fullPath, uint8_t depth, const uint8_t* fog, const game::Monster* monsters,
                       uint8_t monsterCount, const game::Item* items, uint8_t itemCount) {
  FILE* f = fopen(fullPath, "wb");
  uint8_t version = 1;
  fwrite(&version, sizeof(version), 1, f);
  fwrite(&depth, sizeof(depth), 1, f);
  fwrite(fog, 1, game::FOG_SIZE, f);
  // v1 has no door-open bytes here -- this is the point being tested.
  fwrite(&monsterCount, sizeof(monsterCount), 1, f);
  for (uint8_t i = 0; i < monsterCount; i++) fwrite(&monsters[i], sizeof(game::Monster), 1, f);
  fwrite(&itemCount, sizeof(itemCount), 1, f);
  for (uint8_t i = 0; i < itemCount; i++) fwrite(&items[i], sizeof(game::Item), 1, f);
  fclose(f);
}

void testOldSaveNullptrCompatPath(const std::string& sdRoot) {
  constexpr uint8_t kDepth = 12;
  std::string dir = sdRoot + "/.crosspoint/game";
  std::string cmd = "mkdir -p '" + dir + "'";
  system(cmd.c_str());
  char path[256];
  std::snprintf(path, sizeof(path), "%s/level_%02u.bin", dir.c_str(), kDepth);

  std::vector<uint8_t> fog(game::FOG_SIZE);
  fillPattern(fog.data(), game::FOG_SIZE, 0x22);
  game::Monster monsters[1];
  monsters[0] = game::Monster{1, 1, 4, 8, static_cast<uint8_t>(game::MonsterState::Wandering)};
  game::Item items[1];
  items[0] = game::Item{0, 0, static_cast<uint8_t>(game::ItemType::Gold), 0, 3, 0, 0};

  writeV1LevelFile(path, kDepth, fog.data(), monsters, 1, items, 1);

  // Case A: caller passes nullptr for doorOpen (matches the documented
  // contract in GameSave.h) -- must not crash, must still load everything else.
  std::vector<uint8_t> fog2(game::FOG_SIZE, 0);
  game::Monster monsters2[game::MAX_MONSTERS];
  game::Item items2[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount2 = 0, itemCount2 = 0;
  bool okA = GameSave::loadLevel(kDepth, fog2.data(), nullptr, monsters2, monsterCount2, items2, itemCount2);
  CHECK(okA, "loadLevel() with doorOpen=nullptr on a v1 file returned false (should succeed)");
  CHECK(std::memcmp(fog.data(), fog2.data(), game::FOG_SIZE) == 0, "fog corrupted when loading v1 file with doorOpen=nullptr");
  CHECK(monsterCount2 == 1 && monsters2[0].hp == 8, "monster data misaligned when loading v1 file with doorOpen=nullptr -- "
                                                     "the version-gated discard-read is not consuming the right byte count");
  CHECK(itemCount2 == 1 && items2[0].count == 3, "item data misaligned when loading v1 file with doorOpen=nullptr");

  // Case B: caller passes a real (non-null) doorOpen buffer against a v1
  // file -- contract says it's left untouched since v1 never wrote those
  // bytes. Sentinel-fill first and confirm it survives unchanged.
  std::vector<uint8_t> fog3(game::FOG_SIZE, 0);
  std::vector<uint8_t> doorOpen3(game::FOG_SIZE, 0x5A);  // sentinel
  game::Monster monsters3[game::MAX_MONSTERS];
  game::Item items3[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount3 = 0, itemCount3 = 0;
  bool okB = GameSave::loadLevel(kDepth, fog3.data(), doorOpen3.data(), monsters3, monsterCount3, items3, itemCount3);
  CHECK(okB, "loadLevel() with a real doorOpen buffer on a v1 file returned false (should succeed)");
  bool sentinelIntact = true;
  for (int i = 0; i < game::FOG_SIZE; i++) {
    if (doorOpen3[i] != 0x5A) { sentinelIntact = false; break; }
  }
  CHECK(sentinelIntact, "doorOpen buffer was written to when loading a v1 file (should be left untouched per GameSave.h contract)");
  CHECK(monsterCount3 == 1 && itemCount3 == 1, "monster/item counts wrong on the non-null-doorOpen v1 load");

  std::printf("[req6b] v1 (pre-door-persistence) file: nullptr-doorOpen load ok=%d, sentinel-buffer left untouched=%d\n",
              okA, sentinelIntact);
}

// --- Req: top-level SAVE_FILE_VERSION v3 file is cleanly REJECTED by the ------
// --- current v4 loader, not misread as a v4 struct -----------------------
//
// This targets GameState.cpp's top-level SAVE_FILE_VERSION (currently 4), which
// is distinct from GameSave.cpp's per-floor LEVEL_FILE_VERSION already covered
// by testOldSaveNullptrCompatPath() above. Hand-crafts a file with version byte
// 3 followed by a Player-sized block of poison bytes (0xAA), matching the exact
// on-disk shape saveToFile() would have produced under a hypothetical v3 (a
// version byte + raw POD Player struct), then loads it through the REAL,
// current loadFromFile(). Proves: (a) load is refused (returns false), and
// (b) the live in-memory player is NOT corrupted by the rejected read -- the
// version check must reject before the raw-POD Player read, not after.
void testV3SaveRejectedByV4Loader(const std::string& sdRoot) {
  std::string dir = sdRoot + "/.crosspoint/game";
  std::string cmd = "mkdir -p '" + dir + "'";
  system(cmd.c_str());
  std::string path = dir + "/save.bin";

  FILE* f = fopen(path.c_str(), "wb");
  uint8_t version = 3;
  fwrite(&version, sizeof(version), 1, f);
  std::vector<uint8_t> poison(sizeof(game::Player), 0xAA);
  fwrite(poison.data(), 1, poison.size(), f);
  // A trailing inventoryCount byte a real v4 file would also expect next --
  // deliberately poisoned too, so if the version check is somehow bypassed
  // the read would visibly misbehave rather than accidentally look sane.
  uint8_t inventoryCount = 0xAA;
  fwrite(&inventoryCount, sizeof(inventoryCount), 1, f);
  fclose(f);

  // Prime known-good in-memory state via a real newGame() first, so any
  // corruption from the rejected load attempt is directly observable.
  GAME_STATE.newGame(0x1234);
  uint32_t knownHp = GAME_STATE.player.hp;
  uint32_t knownDepth = GAME_STATE.player.dungeonDepth;

  bool loaded = GAME_STATE.loadFromFile();
  CHECK(!loaded, "loadFromFile() returned true for a v3 file against a v4 loader -- should reject");
  CHECK(GAME_STATE.player.hp == knownHp, "in-memory hp corrupted by a rejected v3 load: got %u, expected %u (pre-load newGame value)",
        GAME_STATE.player.hp, knownHp);
  CHECK(GAME_STATE.player.dungeonDepth == knownDepth, "in-memory dungeonDepth corrupted by a rejected v3 load: got %u, expected %u",
        GAME_STATE.player.dungeonDepth, knownDepth);

  std::printf("[save-compat] hand-crafted v3 save file (version byte + poisoned Player bytes) against real v4 "
              "loadFromFile(): loaded=%d (expected 0), in-memory state left intact after rejection\n", loaded);
}

}  // namespace

int main() {
  const std::string sdRoot = "/tmp/game_save_harness_sd";
  resetSdRoot(sdRoot.c_str());
  HalStorage::getInstance().root = sdRoot;

  testCombatRngSurvivesSaveReload();
  testDoorOpenBitmapRoundTrips();
  testOldSaveNullptrCompatPath(sdRoot);
  testV3SaveRejectedByV4Loader(sdRoot);

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
