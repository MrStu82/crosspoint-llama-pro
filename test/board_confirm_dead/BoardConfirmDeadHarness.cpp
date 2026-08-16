// Standalone host harness asserting, against the REAL, unmodified
// BoardConfig::XTEINK_X4_PRO constexpr profile (compiled with
// -DFREEINK_DEVICE_X4PRO=1, the same flag the real x4pro firmware build
// uses), that Button::Confirm has no live trigger path on this board:
//
//   1. input.confirm == PIN_UNASSIGNED  -- no digital GPIO backs Confirm.
//   2. touch.synthesizeConfirm == false -- no touch-tap synthesis fallback
//      backs it either.
//
// This is a data-level proof, not a literal trace of wasReleased(Confirm)
// through MappedInputManager/InputManager/HalGPIO: those depend on real
// Arduino/ESP-IDF APIs (pinMode, Wire, driver/gpio.h's real gpio_get_level,
// etc.) with no host-emulated implementation anywhere in this repo (the only
// CROSSPOINT_EMULATED reference in the tree, lib/hal/HalGPIO.h, has no
// "==1" branch to build against). BoardConfig.h itself is pure constexpr
// data with no such dependency, so it's the deepest point in the real chain
// that can be exercised on host untouched -- and since Confirm's assigned
// pin is PIN_UNASSIGNED regardless of which settings index
// (SETTINGS.frontButtonConfirm) selects it, this data fact alone is
// sufficient: InputManager can never structurally read a real button for
// it, no matter what settings value is in play.
//
// Also asserts input.back == PIN_UNASSIGNED, confirming (not contradicting)
// the backlog note that Back's real trigger is the separate Home-key-tap
// synthesis path (wasHomeKeyBackGesture()), not this struct -- Back and
// Confirm are not symmetric in mechanism, only in outcome (both dead at the
// InputPins level).

#include <cstdio>

#include "BoardConfig.h"

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
  const BoardConfig::BoardProfile& p = BoardConfig::XTEINK_X4_PRO;

  CHECK(p.input.confirm == BoardConfig::PIN_UNASSIGNED,
        "expected XTEINK_X4_PRO.input.confirm == PIN_UNASSIGNED, got %d", p.input.confirm);
  CHECK(p.touch.synthesizeConfirm == false,
        "expected XTEINK_X4_PRO.touch.synthesizeConfirm == false, got %d", p.touch.synthesizeConfirm);

  // Corroborating, not the headline claim: Back is also PIN_UNASSIGNED at
  // this layer -- its live trigger is the separate Home-key gesture path,
  // entirely outside BoardConfig, so Back and Confirm are dead here for
  // different reasons, not the same one.
  CHECK(p.input.back == BoardConfig::PIN_UNASSIGNED,
        "expected XTEINK_X4_PRO.input.back == PIN_UNASSIGNED (rescued elsewhere via Home-key synthesis), got %d",
        p.input.back);
  CHECK(p.touch.hasHomeKey == true,
        "expected XTEINK_X4_PRO.touch.hasHomeKey == true (Back's actual rescue path), got %d", p.touch.hasHomeKey);

  if (failures == 0) {
    printf(
        "PASS: Button::Confirm has no live trigger path on XTEINK_X4_PRO "
        "(input.confirm == PIN_UNASSIGNED, touch.synthesizeConfirm == false)\n");
    return 0;
  }
  printf("FAIL: %d assertion(s) failed\n", failures);
  return 1;
}
