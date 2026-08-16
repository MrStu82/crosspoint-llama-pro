// Narrow replacement for the killed test/corrupt_notice_touch/ full-pipeline harness (parent,
// msg 3946/3948: "bin the full-pipeline render harness -- test
// hitTestCorruptSaveNoticeContinue() directly. Public function, pure geometry, no display, no
// Bitmap, no i18n codegen, no HalFile.").
//
// This harness constructs a bare GameRenderer, calls the real, unmodified
// GameRenderer::initForTest(screenW, screenH) -- which sets screenW/screenH and calls the
// private computeLayout(), with no GfxRenderer instance and no HAL involved at all -- then
// calls the real, unmodified GameRenderer::hitTestCorruptSaveNoticeContinue(x, y) directly.
// corruptNoticeContinueRect() itself stays private (only the hit-test entry point is public,
// the same one GameActivity::loop() actually calls), so the button's bounds are recovered by
// scanning hitTestCorruptSaveNoticeContinue() over the full screen -- same technique as the
// now-dead CorruptNoticeTouchHarness.cpp, adapted to skip GameActivity/GfxRenderer/HalFile
// entirely.
//
// Redness proof (parent, msg 3946/3948): `git stash` the modal-fix diff, rerun this harness
// and observe it fail (or fail to build -- hitTestCorruptSaveNoticeContinue() does not exist
// pre-fix, itself a valid form of "red"), then `git stash pop` and observe it pass. See the
// session report to parent for the actual observed output of both runs.
//
// Build: see test/corrupt_notice_hittest/README.

#include "game/GameRenderer.h"

#include <cstdio>

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
  // Portrait 480x800, matching GameRenderer.h's own "Screen layout (portrait 480x800)"
  // documented constants -- the real on-device logical screen geometry.
  GameRenderer renderer;
  renderer.initForTest(480, 800);
  CHECK(renderer.screenW == 480 && renderer.screenH == 800,
        "expected initForTest(480, 800) to set screenW/screenH accordingly, got w=%d h=%d",
        renderer.screenW, renderer.screenH);

  // Recover the Continue button's bounds by scanning the real public hit-test entry point
  // itself, not by reading the private corruptNoticeContinueRect() -- this only ever calls
  // the exact public entry point production code (GameActivity::loop()'s touch dispatch)
  // actually calls.
  int minX = INT32_MAX, maxX = INT32_MIN, minY = INT32_MAX, maxY = INT32_MIN;
  int hitCount = 0;
  for (int y = 0; y < renderer.screenH; y += 2) {
    for (int x = 0; x < renderer.screenW; x += 2) {
      if (renderer.hitTestCorruptSaveNoticeContinue(x, y)) {
        hitCount++;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
  }
  CHECK(hitCount > 0, "expected hitTestCorruptSaveNoticeContinue() to be true somewhere on screen, found no hits");

  if (failures != 0) {
    printf("FAIL: %d assertion(s) failed (could not locate a Continue hit region -- skipping in/out checks)\n",
           failures);
    return 1;
  }

  const int centerX = (minX + maxX) / 2;
  const int centerY = (minY + maxY) / 2;

  // A point at the discovered rect's center must be a hit.
  CHECK(renderer.hitTestCorruptSaveNoticeContinue(centerX, centerY),
        "expected a point at the discovered rect's center (%d, %d) to hit", centerX, centerY);

  // A point clearly outside the discovered rect (just above its top edge) must NOT be a hit --
  // guards against a degenerate always-true hit test passing this proof for the wrong reason.
  const int outsideY = (minY > 5) ? minY - 5 : -1;
  CHECK(!renderer.hitTestCorruptSaveNoticeContinue(centerX, outsideY),
        "expected a point above the discovered rect's top edge (%d, %d) to NOT hit", centerX, outsideY);

  // Off-screen points must not hit either -- basic sanity on the geometry, not just the
  // scanned window.
  CHECK(!renderer.hitTestCorruptSaveNoticeContinue(-10, -10), "expected an off-screen point (-10, -10) to NOT hit");
  CHECK(!renderer.hitTestCorruptSaveNoticeContinue(renderer.screenW + 10, renderer.screenH + 10),
        "expected an off-screen point (screenW+10, screenH+10) to NOT hit");

  if (failures == 0) {
    printf(
        "PASS: all assertions passed (GameRenderer::hitTestCorruptSaveNoticeContinue() -- "
        "discovered hit region=[x=%d..%d y=%d..%d], center hit, above-top-edge and off-screen "
        "points correctly missed)\n",
        minX, maxX, minY, maxY);
    return 0;
  }
  printf("FAIL: %d assertion(s) failed\n", failures);
  return 1;
}
