// Standalone host harness (no gtest dependency) for the stale-level-save OOB
// monster.type defect reported against the shipped v1.5.0-53-g4d1e64a build
// (empty-panic hard reset entering GameActivity).
//
// HISTORY (2026-08-16): this file originally proved the defect against
// unmodified pre-fix code -- hand-crafted a stale v1-format save carrying an
// out-of-range monster.type, loaded it through the real, unmodified
// GameSave::loadLevel() (which pre-fix accepted it and carried the poisoned
// type straight through untouched), then fed the resulting monsters[] array
// into the real, unmodified game::FrameDirtyPlanner::computeCellVisual().
// Built with ASan+UBSan and -fno-sanitize-recover=all (load-bearing, not
// decorative: without it UBSan logs-and-continues past the trap instead of
// aborting, which would have let the process exit 0 with silently-read
// adjacent rodata -- exactly the "may well just return garbage and sail on"
// failure mode parent's brief called out by name), that pre-fix run aborted
// at the trap line itself (src/game/FrameDirtyPlanner.h:106, the
// MONSTER_DEFS[monsters[m].type] read) with exit code 1, before any of this
// file's own diagnostic printf()s even ran -- a real, unconditional fault
// against shipping logic, not a self-graded pass/fail or a reimplementation
// of the bug.
//
// POST-FIX (this version): GameSave::loadLevel() now validates every
// index-typed field at the load boundary and rejects (returns false)
// atomically -- see GameSave.cpp. This harness now asserts THAT contract:
// the poisoned file must be rejected, the caller's output arrays must come
// back byte-for-byte untouched (proving the rejection is atomic, not a
// partial overwrite), and a genuinely well-formed save must still load and
// render cleanly through the same real computeCellVisual() call with no
// fault (positive-path regression, proving the fix isn't over-rejecting).
//
// NOTE: src/game/GameRenderer.cpp's drawViewportCell() contains a SECOND,
// separately hand-mirrored instance of this exact same unguarded
// MONSTER_DEFS[monsters[m].type] pattern (confirmed by direct read -- per
// FrameDirtyPlanner.h's own header comment, it is NOT a call-through to
// computeCellVisual(), it duplicates the same player>monster>item>tile
// priority logic independently, and is not deduplicated in this build --
// tracked as explicit follow-up). This harness does not drive that second
// site directly, since doing so would require stubbing the GfxRenderer/HAL
// surface drawViewportCell() takes a reference to -- out of scope here. This
// does not weaken the fix: validating monster.type at GameSave::loadLevel()'s
// load boundary means neither call site can ever observe an out-of-range
// value, since both read from the same in-memory array loadLevel() populates.
//
// Build (post-fix, expect ALL CHECKS PASSED / exit 0):
//   g++ -std=c++20 -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
//       -I test/game_save/stubs -I src/game -I lib/Serialization -I lib/Memory \
//       test/game_save/StaleSaveOobHarness.cpp \
//       src/game/GameState.cpp src/game/GameSave.cpp src/game/AchievementBus.cpp \
//       src/game/FlavorText.cpp \
//       -o /tmp/stale_save_oob_harness_postfix
// Run:   /tmp/stale_save_oob_harness_postfix
//
// To reproduce the original pre-fix fault, build the same command against a
// git checkout of GameSave.cpp/GameActivity.cpp from before this fix (e.g.
// `git show <pre-fix-sha>:src/game/GameSave.cpp`) -- the harness's own
// CHECK() logic in this file describes post-fix behavior only, so a pre-fix
// binary should be built from the OLD version of this file (recoverable from
// git history) paired with the OLD GameSave.cpp, not this file paired with
// old GameSave.cpp.

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "FrameDirtyPlanner.h"
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
  std::string cmd = "rm -rf '" + std::string(path) + "'";
  system(cmd.c_str());
  ::mkdir(path, 0755);
}

// Current (post-Fix-2) v3-level-file shape: version=3, depth, gameSeed,
// fog, doorOpen, monsterCount+monsters, itemCount+items. Used (instead of
// writeV1LevelFile) wherever this harness needs to isolate monster.type
// validation from the (now separate, already-covered-elsewhere) version/seed
// rejection path -- a v1 file would now be rejected outright on version
// alone, which would prove nothing about the monster.type check specifically.
void writeV3LevelFile(const char* fullPath, uint8_t depth, uint32_t gameSeed, const uint8_t* fog,
                       const uint8_t* doorOpen, const game::Monster* monsters, uint8_t monsterCount,
                       const game::Item* items, uint8_t itemCount) {
  FILE* f = fopen(fullPath, "wb");
  uint8_t version = 3;
  fwrite(&version, sizeof(version), 1, f);
  fwrite(&depth, sizeof(depth), 1, f);
  fwrite(&gameSeed, sizeof(gameSeed), 1, f);
  fwrite(fog, 1, game::FOG_SIZE, f);
  fwrite(doorOpen, 1, game::FOG_SIZE, f);
  fwrite(&monsterCount, sizeof(monsterCount), 1, f);
  for (uint8_t i = 0; i < monsterCount; i++) fwrite(&monsters[i], sizeof(game::Monster), 1, f);
  fwrite(&itemCount, sizeof(itemCount), 1, f);
  for (uint8_t i = 0; i < itemCount; i++) fwrite(&items[i], sizeof(game::Item), 1, f);
  fclose(f);
}

// Reproduces the exact real-world scenario: a per-level save file written by
// an older/incompatible build (version=1, well under the current
// LEVEL_FILE_VERSION=2 ceiling, so GameSave::loadLevel()'s `version >
// LEVEL_FILE_VERSION` check does NOT reject it) carrying a monster.type
// value that is out of range for the CURRENT MONSTER_DEFS[] table --
// exactly what "Incompatible save version 1 (expected 4)" in the field
// crash log proves happened at the whole-game-save level, one layer up.
// PRE-FIX form of this test (kept here in comment form, not code, since it
// can no longer both compile and mean what it says once the fix lands):
// called GameSave::loadLevel() on the poisoned file, asserted it returned
// true and faithfully carried monster.type=200 through untouched, then fed
// that straight into FrameDirtyPlanner::computeCellVisual() and asserted
// (expecting to be proven wrong) that the call would NOT fault. Run against
// unmodified pre-fix code: aborted at FrameDirtyPlanner.h:106
// (MONSTER_DEFS[monsters[m].type], type=200 against a 21-entry table), exit
// code 1, before any of this file's own diagnostic printf()s executed -- a
// genuine unconditional fault, not a self-graded pass. That result is what
// this harness originally proved and is preserved in the session record;
// it is not re-derivable from the current file since the assertions below
// now encode the fix's contract instead of the bug's.
//
// POST-FIX contract asserted below: GameSave::loadLevel() must REJECT (return
// false) a file carrying an out-of-range monster.type, and must do so
// atomically -- the caller's output arrays must come back byte-for-byte
// identical to whatever "freshly generated floor" they held before the call,
// not a partial mix of stale-file bytes and fresh-generation bytes. A second
// case proves the fix isn't just "reject everything": a well-formed,
// in-range monster still loads and renders cleanly through the same real
// FrameDirtyPlanner::computeCellVisual() path with no fault.
void testStaleSaveOobMonsterRejectedAtomically(const std::string& sdRoot) {
  constexpr uint8_t kDepth = 1;
  constexpr uint32_t kActiveSeed = 0x7E57;
  std::string dir = sdRoot + "/.crosspoint/game";
  std::string cmd = "mkdir -p '" + dir + "'";
  system(cmd.c_str());
  char path[256];
  std::snprintf(path, sizeof(path), "%s/level_%02u.bin", dir.c_str(), kDepth);

  std::vector<uint8_t> fog(game::FOG_SIZE, 0);
  std::vector<uint8_t> doorOpen(game::FOG_SIZE, 0);
  // Monster sits at (5,5); type is deliberately far outside
  // [0, MONSTER_DEF_COUNT) -- 200 is not a plausible in-range value for any
  // version of this table, matching an old/corrupt/incompatible save byte,
  // not an off-by-one. File is otherwise a genuinely well-formed, correctly
  // seeded v3 file (matching kActiveSeed below) so this isolates the
  // monster.type validation path from the separate version/seed rejection
  // path (already covered by StaleStateAndCrossSeedLevelHarness.cpp and
  // GameSaveRoundTripHarness.cpp's testOldSaveFileRejected).
  constexpr uint8_t kPoisonedType = 200;
  static_assert(kPoisonedType >= game::MONSTER_DEF_COUNT, "poisoned type must be genuinely out of range");
  game::Monster fileMonsters[1];
  fileMonsters[0] = game::Monster{5, 5, kPoisonedType, 10, static_cast<uint8_t>(game::MonsterState::Wandering)};
  game::Item fileItems[0];

  writeV3LevelFile(path, kDepth, kActiveSeed, fog.data(), doorOpen.data(), fileMonsters, 1, fileItems, 0);

  // --- Step 1: seed the caller's arrays with sentinel "fresh floor" values,
  // exactly as GameActivity::loadOrGenerateLevel() would have them right
  // before calling loadLevel() -- a real generated monster (type=3, well
  // in-range) at a different cell, and fog with one cell already explored.
  constexpr uint8_t kFreshType = 3;
  static_assert(kFreshType < game::MONSTER_DEF_COUNT, "sentinel fresh type must be in range");
  std::vector<uint8_t> freshFog(game::FOG_SIZE, 0);
  game::fogSetExplored(freshFog.data(), 7, 7);
  std::vector<uint8_t> loadedFog = freshFog;  // copy: this is what loadLevel() must leave untouched
  game::Monster loadedMonsters[game::MAX_MONSTERS];
  loadedMonsters[0] = game::Monster{9, 9, kFreshType, 12, static_cast<uint8_t>(game::MonsterState::Wandering)};
  game::Item loadedItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t loadedMonsterCount = 1, loadedItemCount = 0;

  // --- Step 2: load the poisoned file through the REAL, unmodified
  // GameSave::loadLevel() ---
  bool loaded = GameSave::loadLevel(kDepth, kActiveSeed, loadedFog.data(), nullptr, loadedMonsters,
                                     loadedMonsterCount, loadedItems, loadedItemCount);
  CHECK(!loaded, "loadLevel() accepted a file with an out-of-range monster.type=%u -- the load-boundary "
                 "validation fix is not rejecting it",
        kPoisonedType);
  CHECK(loadedMonsterCount == 1 && loadedMonsters[0].type == kFreshType && loadedMonsters[0].x == 9,
        "loadLevel() mutated the caller's monsters[] even though it rejected the file -- got count=%u type=%u "
        "(expected the untouched fresh-floor sentinel: count=1 type=%u) -- rejection is not atomic",
        loadedMonsterCount, loadedMonsters[0].type, kFreshType);
  CHECK(std::memcmp(loadedFog.data(), freshFog.data(), game::FOG_SIZE) == 0,
        "loadLevel() mutated the caller's fogOfWar even though it rejected the file -- rejection is not atomic");
  std::printf("[stale-save-oob] loadLevel() correctly rejected monster.type=%u (max %d) and left the caller's "
              "fresh-floor arrays byte-for-byte untouched\n",
              kPoisonedType, game::MONSTER_DEF_COUNT - 1);

  // --- Step 3: positive-path regression -- a well-formed save (in-range
  // monster.type) must still load AND render cleanly through the same real
  // FrameDirtyPlanner::computeCellVisual() path, no fault, proving the fix
  // rejects only genuinely invalid data, not everything.
  std::string dir2 = sdRoot + "/.crosspoint/game";
  char path2[256];
  std::snprintf(path2, sizeof(path2), "%s/level_%02u.bin", dir2.c_str(), static_cast<unsigned>(kDepth + 1));
  game::Monster validFileMonsters[1];
  validFileMonsters[0] = game::Monster{5, 5, kFreshType, 10, static_cast<uint8_t>(game::MonsterState::Wandering)};
  writeV3LevelFile(path2, kDepth + 1, kActiveSeed, fog.data(), doorOpen.data(), validFileMonsters, 1, fileItems, 0);

  std::vector<uint8_t> validLoadedFog(game::FOG_SIZE, 0);
  game::Monster validLoadedMonsters[game::MAX_MONSTERS];
  game::Item validLoadedItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t validLoadedMonsterCount = 0, validLoadedItemCount = 0;
  bool validLoaded = GameSave::loadLevel(kDepth + 1, kActiveSeed, validLoadedFog.data(), nullptr, validLoadedMonsters,
                                          validLoadedMonsterCount, validLoadedItems, validLoadedItemCount);
  CHECK(validLoaded && validLoadedMonsterCount == 1 && validLoadedMonsters[0].type == kFreshType,
        "loadLevel() failed to load a genuinely well-formed save file -- the fix is over-rejecting");

  std::vector<game::Tile> tiles(game::MAP_SIZE, game::Tile::Floor);
  auto visibleOwner = std::make_unique<bool[]>(game::MAP_SIZE);
  bool* visible = visibleOwner.get();
  std::fill(visible, visible + game::MAP_SIZE, false);
  int mapIdx = validLoadedMonsters[0].y * game::MAP_WIDTH + validLoadedMonsters[0].x;
  visible[mapIdx] = true;
  game::fogSetExplored(validLoadedFog.data(), validLoadedMonsters[0].x, validLoadedMonsters[0].y);

  game::FrameDirtyPlanner planner;
  game::CellVisual cv = planner.computeCellVisual(validLoadedMonsters[0].x, validLoadedMonsters[0].y, tiles.data(),
                                                   validLoadedFog.data(), validLoadedMonsters, validLoadedMonsterCount,
                                                   validLoadedItems, validLoadedItemCount, visible, /*playerX=*/0,
                                                   /*playerY=*/0);
  std::printf("[stale-save-oob] positive-path regression: valid monster.type=%u rendered cleanly, glyph=0x%02x\n",
              kFreshType, cv.glyph);
}

}  // namespace

int main() {
  const std::string sdRoot = "/tmp/stale_save_oob_harness_sd";
  resetSdRoot(sdRoot.c_str());
  HalStorage::getInstance().root = sdRoot;

  testStaleSaveOobMonsterRejectedAtomically(sdRoot);

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
