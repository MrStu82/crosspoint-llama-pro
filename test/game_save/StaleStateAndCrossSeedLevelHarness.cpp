// Standalone host harness (no gtest dependency) proving two defects parent
// flagged (msg 3918) against Stuart's field report of an empty-panic/garbage
// state after a corrupt save.bin: on this session's investigation these are
// NOT one bug but two independent load-boundary gaps, plus one unrelated
// rendering nit (drawLine stroke-width off-by-one, not covered here -- no
// state to characterise, it's a pure geometry fix, see report to parent).
//
// THIS FILE IS PRE-FIX. Every CHECK() below encodes the desired POST-FIX
// contract and is expected to FAIL (go red) against the current, unmodified
// production code -- that is the point: it proves the gap exists before any
// production line is touched, per parent's "harness red third, code fourth"
// instruction. Built and run against real, unmodified src/game/GameState.cpp
// and src/game/GameSave.cpp (same technique as GameSaveRoundTripHarness.cpp
// and StaleSaveOobHarness.cpp in this directory -- only HalStorage.h/Logging.h
// are swapped for host stubs via the include path).
//
// Defect 1 (GameActivity.cpp:108-111): GameActivity::onEnter() calls
// `GAME_STATE.loadFromFile()` and discards its return value. GameState.cpp
// confirms loadFromFile() is atomic on rejection -- it leaves the live
// GameState singleton completely untouched, logging "Save file rejected: X"
// and returning false. Untouched is the problem: this GameState instance is
// a persistent singleton, so "untouched" does not mean "safe defaults", it
// means "still holding whatever the PREVIOUS run left it as" (an old,
// possibly-finished run's depth/hp/inventory/messages). onEnter()'s own
// comment ("If no save was loaded, newGame() was already called before
// entering this activity") is only true on the hasSaveFile()==false path
// (see GameTitleActivity::loop()) -- it does NOT hold on the
// hasSaveFile()==true-but-loadFromFile()-fails path, which is exactly
// Stuart's corrupt-save scenario and has no newGame() call anywhere on it.
// testCorruptSaveFallsThroughToStaleState() below reproduces this precisely:
// a prior run's state is established via the real GAME_STATE.newGame() +
// mutation, a corrupt save.bin (bad version byte) is written to represent
// Stuart's file, then onEnter()'s exact two-line sequence
// (`if (hasSaveFile()) loadFromFile();`, return value discarded) is executed
// against the REAL functions. Post-fix, GameState must end up in a fresh,
// playable state (or the run must be blocked pending user
// acknowledgement) -- NOT the stale leftover run.
//
// Defect 2 (GameSave.cpp / level_NN.bin format): the level file format has
// no seed field at all -- LEVEL_FILE_VERSION's header is
// [version][depth][fog][door][monsters][items], nothing else. GameSave has
// no way to know which logical run (which gameSeed) a level_NN.bin belongs
// to. In practice: run A (seed=1111) reaches depth 3 and saves. The run
// exits without a clean run-save delete (crash, or exactly the corrupt-save
// scenario above forcing a fresh run while old level files survive on disk).
// Run B starts fresh with a different seed (2222) and, on reaching depth 3,
// generates its OWN (differently seeded) tile layout via
// DungeonGenerator::generate(), then unconditionally overlays run A's
// on-disk fog/door/monsters/items on top of it via GameSave::loadLevel() --
// producing a level whose walkable-tile layout belongs to one seed and whose
// monster/item positions belong to a different seed's layout (monsters can
// end up inside walls, items unreachable, etc; this is the second half of
// what a "garbage state" field report looks like from the outside).
// testCrossSeedLevelFileAcceptedByCurrentLoader() below reproduces exactly
// this: real saveLevel() call under simulated run A, real loadLevel() call
// under simulated run B's different seed. Post-fix, GameSave::loadLevel()
// must reject a level file stamped with a different gameSeed than the
// caller's active run (mirroring the existing monster.type/version rejection
// path StaleSaveOobHarness.cpp already covers), leaving the caller's
// freshly-generated floor authoritative exactly as an out-of-range
// monster.type does today.
//
// A third function, testWellFormedSameSeedRoundTripStillWorks(), is the
// positive-path regression: a normal same-seed save/reload must keep working
// once Fix 2 lands. This one is expected to PASS both pre-fix and post-fix
// (it exercises no rejection path) -- included so Fix 2's implementation has
// an immediate "did I break the ordinary case" signal alongside the two red
// checks above.
//
// Build (pre-fix, expect testCorruptSaveFallsThroughToStaleState and
// testCrossSeedLevelFileAcceptedByCurrentLoader to FAIL/print FAIL lines;
// testWellFormedSameSeedRoundTripStillWorks to pass):
//   g++ -std=c++20 -O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
//       -I test/game_save/stubs -I src/game -I lib/Serialization -I lib/Memory \
//       test/game_save/StaleStateAndCrossSeedLevelHarness.cpp \
//       src/game/GameState.cpp src/game/GameSave.cpp src/game/AchievementBus.cpp \
//       -o /tmp/stale_state_cross_seed_harness
// Run:   /tmp/stale_state_cross_seed_harness

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
  std::string cmd = "rm -rf '" + std::string(path) + "'";
  system(cmd.c_str());
  ::mkdir(path, 0755);
}

// --- Defect 1: corrupt save.bin falls through to stale prior-run state ----
//
// Mirrors GameActivity::onEnter()'s exact sequence (GameActivity.cpp:108-111):
//   if (GAME_STATE.hasSaveFile()) { GAME_STATE.loadFromFile(); }
// against the REAL GameState singleton and REAL file I/O -- not a
// reimplementation or description of the bug.
void testCorruptSaveFallsThroughToStaleState() {
  // Step 1: establish "previous run" state via the real newGame() + real
  // in-run mutation, exactly as a long, nearly-finished run would look.
  constexpr uint32_t kStaleSeed = 0xAAAA5555;
  GAME_STATE.newGame(kStaleSeed);
  GAME_STATE.player.dungeonDepth = 15;
  GAME_STATE.player.hp = 3;
  GAME_STATE.player.turnCount = 5000;
  GAME_STATE.player.gold = 999;
  GAME_STATE.addMessage("You are gravely wounded!");

  // Step 2: write a corrupt save.bin -- bad version byte, same shape as
  // Stuart's field file (an old/incompatible build's save). Uses the real
  // saveToFile() first to get a byte-correct v4 file, then flips only the
  // version byte -- this is not a synthetic/truncated file, it is a
  // genuinely well-formed-except-for-version file, the same class
  // parseSaveFile() already logs as reason="version".
  CHECK(GAME_STATE.saveToFile(), "setup: saveToFile() returned false, cannot proceed");
  {
    // No openFileForReadWrite() on the real Storage HAL (nor its test stub) --
    // read the whole file out, flip the version byte in memory, and rewrite
    // it whole via the same openFileForWrite() path GameState.cpp itself uses.
    HalFile rf;
    CHECK(Storage.openFileForRead("TEST", "/.crosspoint/game/save.bin", rf),
          "setup: could not reopen save.bin to corrupt it");
    std::vector<uint8_t> bytes(4096);
    int n = rf.read(bytes.data(), bytes.size());
    CHECK(n > 0, "setup: save.bin read back empty, cannot proceed");
    rf.close();
    bytes[0] = 99;  // version byte is the first byte of the file (see GameState.cpp saveToFile())
    HalFile wf;
    CHECK(Storage.openFileForWrite("TEST", "/.crosspoint/game/save.bin", wf),
          "setup: could not reopen save.bin for corrupting rewrite");
    wf.write(bytes.data(), static_cast<size_t>(n));
  }
  CHECK(GameState::validateSaveFile().status == SaveValidity::Status::Invalid,
        "setup: corrupted save.bin did not actually validate as Invalid -- corruption step is broken");

  // Step 3: this is the ACTUAL bug reproduction -- GameActivity::onEnter()'s
  // exact two lines, verbatim control flow, against the real functions.
  if (GAME_STATE.hasSaveFile()) {
    GAME_STATE.loadFromFile();  // return value discarded, exactly as GameActivity.cpp:109 does today
  }

  // Step 4: post-fix contract -- the player must NOT end up playing the
  // stale prior run's leftover state. A fresh newGame() (fresh seed, depth
  // 1, hp==maxHp, turnCount 0) is the minimum acceptable outcome; a fix that
  // instead routes to a blocking notice screen before any play begins would
  // also satisfy "not silently playing stale state", but that is a
  // GameActivity-level (not GameState-level) concern this harness cannot
  // observe directly -- so this check is deliberately strict about the
  // GameState-observable half of the contract: whatever GameActivity does,
  // GAME_STATE itself must not still equal the stale run once play (if any)
  // begins.
  bool stillStale = GAME_STATE.player.dungeonDepth == 15 && GAME_STATE.player.hp == 3 &&
                    GAME_STATE.player.turnCount == 5000 && GAME_STATE.player.gameSeed == kStaleSeed;
  CHECK(!stillStale,
        "GameState still holds the stale prior run's exact state (depth=15 hp=3 turn=5000 seed=0x%08X) after a "
        "corrupt save.bin was rejected -- GameActivity::onEnter() discards loadFromFile()'s return value and never "
        "calls newGame() or blocks play on this path, so a corrupt save.bin silently continues the PREVIOUS run "
        "instead of starting fresh or notifying the player",
        kStaleSeed);
}

// --- Defect 2: a level file from a different seed's run is silently accepted
void testCrossSeedLevelFileAcceptedByCurrentLoader() {
  constexpr uint8_t kDepth = 3;
  constexpr uint32_t kSeedRunA = 1111;
  constexpr uint32_t kSeedRunB = 2222;

  // Run A: reaches depth 3, saves real level state (a monster + explored fog
  // cell) via the real GameSave::saveLevel().
  std::vector<uint8_t> fogA(game::FOG_SIZE, 0);
  game::fogSetExplored(fogA.data(), 4, 4);
  std::vector<uint8_t> doorA(game::FOG_SIZE, 0);
  game::Monster monstersA[1] = {{4, 4, /*type=*/2, /*hp=*/8, static_cast<uint8_t>(game::MonsterState::Wandering)}};
  game::Item itemsA[0];
  CHECK(GameSave::saveLevel(kDepth, kSeedRunA, fogA.data(), doorA.data(), monstersA, 1, itemsA, 0),
        "setup: run A's saveLevel() returned false, cannot proceed");

  // Run B: a genuinely different run (different seed), reaching the same
  // depth fresh. Simulates loadOrGenerateLevel()'s exact overlay sequence --
  // fresh sentinel "just generated" arrays, then loadLevel() is asked to
  // overlay saved state on top.
  constexpr uint8_t kFreshType = 5;
  std::vector<uint8_t> loadedFog(game::FOG_SIZE, 0);
  std::vector<uint8_t> loadedDoor(game::FOG_SIZE, 0);
  game::Monster loadedMonsters[game::MAX_MONSTERS];
  loadedMonsters[0] = game::Monster{9, 9, kFreshType, 12, static_cast<uint8_t>(game::MonsterState::Wandering)};
  game::Item loadedItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t loadedMonsterCount = 1, loadedItemCount = 0;

  bool loaded = GameSave::loadLevel(kDepth, kSeedRunB, loadedFog.data(), loadedDoor.data(), loadedMonsters,
                                    loadedMonsterCount, loadedItems, loadedItemCount);

  // Post-fix contract: GameSave must know this level file was written under
  // a different gameSeed (1111) than run B's active seed (2222) and reject
  // it, leaving run B's freshly-generated sentinel state untouched.
  CHECK(!loaded,
        "GameSave::loadLevel() accepted a level file saved under a DIFFERENT run's seed (0x%X) with no seed check "
        "at all -- run B's freshly-generated depth-%u floor was silently overwritten with run A's leftover "
        "fog/monster state from an unrelated seed",
        kSeedRunA, kDepth);
  CHECK(loadedMonsterCount == 1 && loadedMonsters[0].type == kFreshType && loadedMonsters[0].x == 9,
        "loadLevel() mutated run B's caller-owned monsters[] even though the file belongs to a different seed's run "
        "-- got count=%u type=%u (expected untouched fresh-floor sentinel: count=1 type=%u)",
        loadedMonsterCount, loadedMonsters[0].type, kFreshType);
}

// --- Positive-path regression: ordinary same-seed save/reload still works -
void testWellFormedSameSeedRoundTripStillWorks() {
  constexpr uint8_t kDepth = 7;
  constexpr uint8_t kType = 1;
  constexpr uint32_t kSeed = 4242;

  std::vector<uint8_t> fog(game::FOG_SIZE, 0);
  game::fogSetExplored(fog.data(), 2, 2);
  std::vector<uint8_t> door(game::FOG_SIZE, 0);
  game::Monster monsters[1] = {{2, 2, kType, 10, static_cast<uint8_t>(game::MonsterState::Wandering)}};
  game::Item items[0];
  CHECK(GameSave::saveLevel(kDepth, kSeed, fog.data(), door.data(), monsters, 1, items, 0),
        "setup: saveLevel() returned false, cannot proceed");

  std::vector<uint8_t> loadedFog(game::FOG_SIZE, 0);
  std::vector<uint8_t> loadedDoor(game::FOG_SIZE, 0);
  game::Monster loadedMonsters[game::MAX_MONSTERS];
  game::Item loadedItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t loadedMonsterCount = 0, loadedItemCount = 0;
  bool loaded = GameSave::loadLevel(kDepth, kSeed, loadedFog.data(), loadedDoor.data(), loadedMonsters,
                                    loadedMonsterCount, loadedItems, loadedItemCount);
  CHECK(loaded, "ordinary same-run reload was rejected -- Fix 2 must not reject a level file whose seed matches the "
               "active run");
  CHECK(loadedMonsterCount == 1 && loadedMonsters[0].type == kType && loadedMonsters[0].x == 2,
        "ordinary same-run reload did not faithfully restore saved monster state");
}

}  // namespace

int main() {
  const std::string sdRoot = "/tmp/stale_state_cross_seed_harness_sd";
  resetSdRoot(sdRoot.c_str());
  HalStorage::getInstance().root = sdRoot;

  testCorruptSaveFallsThroughToStaleState();

  resetSdRoot(sdRoot.c_str());
  HalStorage::getInstance().root = sdRoot;
  testCrossSeedLevelFileAcceptedByCurrentLoader();

  resetSdRoot(sdRoot.c_str());
  HalStorage::getInstance().root = sdRoot;
  testWellFormedSameSeedRoundTripStillWorks();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
