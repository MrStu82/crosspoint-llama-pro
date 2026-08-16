// Standalone host harness driving the REAL, unmodified GameActivity through
// the whole-run corrupt-save flow: onEnter() finds a present-but-unloadable
// save.bin -> CorruptSaveNotice/WholeRun modal -> a scripted Confirm release
// -> loop() -> the real resolveWholeRunCorruptNotice(). No pixel rendering is
// involved (GfxRenderer/GameRenderer are stubbed, see stubs/), so this only
// proves the control-flow + GAME_STATE side effects, not any drawn frame.
//
// GameActivity/Activity/GameState/GameSave/DungeonGenerator/AchievementBus/
// FlavorText are compiled for real (via mirror/ symlinks for the two
// activities files). screenMode/corruptNoticeScope/corruptNoticeSelection are
// all private on GameActivity with no accessor -- per the sponsor_hp_clamp
// precedent, this harness never adds a friend/accessor and instead asserts
// only through observable PUBLIC side effects: GAME_STATE.player fields after
// newGame(), and real presence/absence of the stub-filesystem save file.

#include <cstdio>
#include <cstring>
#include <string>

#include "activities/Activity.h"
#include "activities/game/GameActivity.h"
#include "game/GameState.h"

// HalStorage.h is pulled in transitively by GameState.h's users (GameState.cpp
// etc.), but the harness needs Storage/HalFile directly to hand-write a
// corrupt save.bin and to inspect fake_sd afterwards -- include explicitly.
#include <HalStorage.h>

namespace {

int failures = 0;

}  // namespace

#define CHECK(cond, ...)                    \
  do {                                       \
    if (!(cond)) {                           \
      fprintf(stderr, "FAIL: " __VA_ARGS__); \
      fprintf(stderr, "\n");                 \
      failures++;                            \
    }                                        \
  } while (0)

int main() {
  // Point the stub filesystem at a real dir under this test's own build/
  // output (never /tmp, per project rule). Fresh per run: wipe any leftovers
  // from a prior invocation so hasSaveFile()/deleteSaveFile() assertions
  // below are never polluted by stale state.
  Storage.root = "test/whole_run_corrupt_save/build/fake_sd";
  std::string wipeCmd = "rm -rf " + Storage.root + " && mkdir -p " + Storage.root;
  CHECK(system(wipeCmd.c_str()) == 0, "failed to reset fake_sd scratch dir");

  // Hand-craft a present-but-unloadable save.bin: GameState.cpp's real
  // parseSaveFile() rejects any version byte != SAVE_FILE_VERSION (currently
  // 4) with reason "version" -- write a byte that can never match, so
  // GAME_STATE.hasSaveFile() is true and GAME_STATE.loadFromFile() is
  // guaranteed false, without needing to know the real version constant's
  // exact current value or duplicate any of its layout.
  {
    Storage.mkdir("/.crosspoint/game");
    HalFile f;
    CHECK(Storage.openFileForWrite("TEST", "/.crosspoint/game/save.bin", f), "could not create fake save.bin");
    uint8_t badVersion = 0xFF;
    f.write(&badVersion, sizeof(badVersion));
    f.close();
  }

  CHECK(GAME_STATE.hasSaveFile(), "expected hasSaveFile() true after writing fake save.bin");

  // Real GameActivity, stub renderer/input -- exactly the sponsor_hp_clamp
  // construction pattern.
  GfxRenderer renderer;
  MappedInputManager input;
  GameActivity activity(renderer, input);

  // Real onEnter(): expected to hit GAME_STATE.hasSaveFile() &&
  // !GAME_STATE.loadFromFile(), landing in the CorruptSaveNotice/WholeRun
  // modal. screenMode/corruptNoticeScope are private -- asserted indirectly
  // below via loop()'s observable behavior instead of read directly here.
  activity.onEnter();

  // GameState::loadFromFile() itself calls newGame() internally on any
  // rejected parse (see GameState.cpp), so the player is already reset to
  // defaults at this point -- not yet proof of which code path ran, just
  // establishes the baseline before the Confirm-driven resolve below.
  CHECK(GAME_STATE.player.turnCount == 0, "expected turnCount==0 after initial rejected load");

  // Simulate a Confirm button release and drive the real loop(). If onEnter()
  // had NOT landed in CorruptSaveNotice/WholeRun (e.g. a regression made it
  // fall through to normal play), this Confirm would either be silently
  // ignored by the Playing-mode branch or drive unrelated game logic --
  // either way the post-conditions below (save file deleted, fresh newGame()
  // state) would fail to hold, which is how this indirectly proves the modal
  // branch was actually taken.
  input.nextReleased = MappedInputManager::Button::Confirm;
  activity.loop();

  // --- Post-conditions: resolveWholeRunCorruptNotice(purge=true) ran ---
  // corruptNoticeSelection defaults to 0 (Purge highlighted by default, per
  // GameActivity.cpp's onEnter() comment), so Confirm on first frame resolves
  // with purge=true.

  // 1. GAME_STATE.deleteSaveFile() actually removed the stub save file.
  CHECK(!GAME_STATE.hasSaveFile(), "expected save.bin deleted after purge-confirm");
  CHECK(!Storage.exists("/.crosspoint/game/save.bin"), "expected save.bin physically absent from fake_sd");

  // 2. GAME_STATE.newGame() produced fresh player state.
  CHECK(GAME_STATE.player.turnCount == 0, "expected fresh turnCount==0 after resolveWholeRunCorruptNotice");
  CHECK(GAME_STATE.player.hp == 20, "expected fresh hp==20 after resolveWholeRunCorruptNotice, got %u",
        GAME_STATE.player.hp);
  CHECK(GAME_STATE.player.maxHp == 20, "expected fresh maxHp==20 after resolveWholeRunCorruptNotice, got %u",
        GAME_STATE.player.maxHp);
  CHECK(GAME_STATE.player.dungeonDepth == 1, "expected fresh dungeonDepth==1, got %u",
        GAME_STATE.player.dungeonDepth);

  // 3. screenMode flipped back to Playing is only observable indirectly: a
  // second loop() call with no CorruptSaveNotice-specific input pending
  // should now be safe to run without touching corruptNoticeSelection/modal
  // state again (if screenMode were still CorruptSaveNotice, a stray Up/Down
  // release would have flipped corruptNoticeSelection and left the modal
  // active -- instead exercise a a harmless Confirm no-op release and confirm
  // no crash and state remains resolved, which is the strongest available
  // proof without a private accessor).
  input.nextReleased = MappedInputManager::Button::Confirm;
  activity.loop();
  CHECK(!GAME_STATE.hasSaveFile(), "expected resolved state to remain stable after a second loop() call");

  if (failures == 0) {
    printf("PASS: all assertions passed (whole-run corrupt save -> Confirm -> resolveWholeRunCorruptNotice)\n");
    return 0;
  }
  printf("FAIL: %d assertion(s) failed\n", failures);
  return 1;
}
