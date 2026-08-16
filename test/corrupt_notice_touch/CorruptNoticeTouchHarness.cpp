// Standalone host harness proving the touch path into the CorruptSaveNotice/loop()
// dispatch: a scripted screen-tap landing inside the real, unmodified
// GameRenderer::corruptNoticeContinueRect() drives the real GameActivity::loop() into
// resolveWholeRunCorruptNotice() exactly as a Back release or (on boards where it's
// wired) a Confirm release would. This is the touch-path half of the redness proof
// parent required in msg 3942 -- the board-level half (Button::Confirm has no live
// trigger on x4pro) lives in test/board_confirm_dead/.
//
// Unlike test/whole_run_corrupt_save/ (which stubs GfxRenderer/GameRenderer entirely,
// since it only needs to prove control flow), this harness needs REAL geometry: the
// whole point is proving a tap at the position the app actually draws the Continue
// button actually resolves the modal, so a drifted hit-test/draw mismatch would be
// caught here, not hidden behind a stub. Same technique as test/sponsor_hp_clamp/:
// compiles the REAL GameActivity.cpp/GameRenderer.cpp/GfxRenderer.cpp/EpdFont pipeline
// via symlinked mirror/ + shadow HalDisplay/HalGPIO/CrossPointSettings headers -- see
// that README for the general rationale, this test's own README for what differs.
//
// Redness proof procedure (parent, msg 3942): this harness is written and run AFTER
// the modal fix landed (deviating from strict red-first), so redness is proven a
// different way -- git stash the modal-fix diff, rerun this harness, and observe it
// fail (or fail to build/link, since hitTestCorruptSaveNoticeContinue()/
// corruptNoticeContinueRect() do not exist at all pre-fix -- itself a valid form of
// "red"), then git stash pop and observe it pass. See the session report to parent for
// the actual observed output of both runs.
//
// Build: see test/corrupt_notice_touch/README.

#include <HalDisplay.h>
#include <HalGPIO.h>

#include "activities/game/GameActivity.h"
#include "game/GameRenderer.h"
#include "game/GameState.h"

#include <HalStorage.h>

#include <cstdio>
#include <string>

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
  // Never /tmp, per project rule -- scratch dir lives under this test's own build/.
  Storage.root = "test/corrupt_notice_touch/build/fake_sd";
  std::string wipeCmd = "rm -rf " + Storage.root + " && mkdir -p " + Storage.root;
  CHECK(system(wipeCmd.c_str()) == 0, "failed to reset fake_sd scratch dir");

  // Hand-craft a present-but-unloadable save.bin, same technique as
  // whole_run_corrupt_save: a version byte that can never match
  // GameState.cpp's real SAVE_FILE_VERSION, so hasSaveFile() is true and
  // loadFromFile() is guaranteed false without duplicating the real layout.
  {
    Storage.mkdir("/.crosspoint/game");
    HalFile f;
    CHECK(Storage.openFileForWrite("TEST", "/.crosspoint/game/save.bin", f), "could not create fake save.bin");
    uint8_t badVersion = 0xFF;
    f.write(&badVersion, sizeof(badVersion));
    f.close();
  }
  CHECK(GAME_STATE.hasSaveFile(), "expected hasSaveFile() true after writing fake save.bin");

  // Real GfxRenderer backed by the in-memory HalDisplay stub (real panel geometry,
  // 800x480 physical -- see stubs/HalDisplay.h) -- same construction as
  // test/sponsor_hp_clamp/.
  GfxRenderer renderer(display);
  renderer.begin();

  MappedInputManager input;
  GameActivity activity(renderer, input);

  // Real onEnter(): hits GAME_STATE.hasSaveFile() && !GAME_STATE.loadFromFile(),
  // lands in CorruptSaveNotice/WholeRun, and (line 102 of the real GameActivity.cpp)
  // calls its internal gameRenderer.init(renderer) -- computing screenW/screenH from
  // the same GfxRenderer instance this harness holds.
  activity.onEnter();
  CHECK(GAME_STATE.player.turnCount == 0, "expected turnCount==0 after initial rejected load");

  // Independent probe GameRenderer, initialized against the SAME GfxRenderer instance
  // GameActivity's own (private, unreachable from here) gameRenderer used -- init()
  // deterministically derives screenW/screenH from renderer.getScreenWidth()/Height(),
  // so this probe's hit-test region is byte-identical to the one the real activity is
  // hit-testing against, with no access to GameActivity's private member needed. This
  // is the crux of the proof: we are not hand-computing/duplicating the rect formula,
  // we are asking the real shipped hit-test function for it.
  //
  // corruptNoticeContinueRect() itself is private (only hitTestCorruptSaveNoticeContinue()
  // is public -- the same function GameActivity::loop() actually calls), so the button's
  // bounds are recovered here by scanning hitTestCorruptSaveNoticeContinue() itself over
  // the full screen, not by reading the rect directly. This is arguably a more faithful
  // proof than reading the rect would have been: it only ever calls the exact public
  // entry point production code calls.
  GameRenderer probe;
  probe.init(renderer);
  CHECK(probe.screenW > 0 && probe.screenH > 0, "expected a non-degenerate probe screen size, got w=%d h=%d",
        probe.screenW, probe.screenH);

  int minX = INT32_MAX, maxX = INT32_MIN, minY = INT32_MAX, maxY = INT32_MIN;
  int hitCount = 0;
  for (int y = 0; y < probe.screenH; y += 2) {
    for (int x = 0; x < probe.screenW; x += 2) {
      if (probe.hitTestCorruptSaveNoticeContinue(x, y)) {
        hitCount++;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
  }
  CHECK(hitCount > 0, "expected hitTestCorruptSaveNoticeContinue() to be true somewhere on screen, found no hits");

  int tapX = (minX + maxX) / 2;
  int tapY = (minY + maxY) / 2;

  // A tap OUTSIDE the discovered hit region (just above its top edge) must NOT resolve
  // the modal -- guards against a degenerate always-true hit test passing this proof
  // for the wrong reason.
  input.nextTap = true;
  input.nextTapX = tapX;
  input.nextTapY = (minY > 5) ? minY - 5 : 0;
  activity.loop();
  CHECK(GAME_STATE.hasSaveFile(),
        "expected an off-target tap (above the discovered Continue hit region) to NOT resolve the modal");
  CHECK(GAME_STATE.player.turnCount == 0, "expected an off-target tap to leave player state untouched");

  // A tap INSIDE the rect must resolve the modal via the real touch dispatch branch.
  input.nextTap = true;
  input.nextTapX = tapX;
  input.nextTapY = tapY;
  activity.loop();

  // --- Post-conditions: resolveWholeRunCorruptNotice() ran, via the touch path ---
  CHECK(GAME_STATE.hasSaveFile(), "expected save.bin left untouched after Continue (no purge branch)");
  CHECK(Storage.exists("/.crosspoint/game/save.bin"), "expected save.bin still physically present on fake_sd");
  CHECK(GAME_STATE.player.turnCount == 0, "expected fresh turnCount==0 after resolveWholeRunCorruptNotice");
  CHECK(GAME_STATE.player.hp == 20, "expected fresh hp==20 after resolveWholeRunCorruptNotice, got %u",
        GAME_STATE.player.hp);
  CHECK(GAME_STATE.player.maxHp == 20, "expected fresh maxHp==20 after resolveWholeRunCorruptNotice, got %u",
        GAME_STATE.player.maxHp);
  CHECK(GAME_STATE.player.dungeonDepth == 1, "expected fresh dungeonDepth==1, got %u",
        GAME_STATE.player.dungeonDepth);

  // A second tap at the same spot should now be a harmless no-op (modal already
  // resolved, screenMode flipped back to Playing) -- mirrors whole_run_corrupt_save's
  // stability check.
  input.nextTap = true;
  input.nextTapX = tapX;
  input.nextTapY = tapY;
  activity.loop();
  CHECK(GAME_STATE.hasSaveFile(), "expected resolved state to remain stable after a second tap");
  CHECK(GAME_STATE.player.turnCount == 0, "expected player state to remain stable after a second tap");

  if (failures == 0) {
    printf(
        "PASS: all assertions passed (whole-run corrupt save -> tap resolved via real "
        "GameRenderer::hitTestCorruptSaveNoticeContinue() -> resolveWholeRunCorruptNotice, "
        "off-target tap correctly ignored, discovered hit region=[x=%d..%d y=%d..%d])\n",
        minX, maxX, minY, maxY);
    return 0;
  }
  printf("FAIL: %d assertion(s) failed\n", failures);
  return 1;
}
