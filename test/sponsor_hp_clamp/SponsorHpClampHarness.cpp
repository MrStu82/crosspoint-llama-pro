// Standalone host harness, no gtest, compiled directly with g++. Compiles the REAL,
// unmodified GameActivity.cpp (via test/sponsor_hp_clamp/mirror/, same shadow-header
// technique as test/game_title_render/ -- see that README's "Why mirror/ exists") against
// the REAL GfxRenderer/EpdFont chain, real DungeonGenerator/GameState/GameSave/
// AchievementBus/FlavorText, and a shadow (never-invoked) GameMenuActivity so
// loadOrGenerateLevel() links and runs as shipped.
//
// Targets the regression flagged in review at commit ad370e9: a +maxHp sponsor from the
// previous floor can leave hp above the new floor's effective cap once that sponsor is
// gone, and loadOrGenerateLevel() must clamp p.hp down to game::effectiveMaxHp(p)
// immediately after rolling the new floor's sponsor -- not leave the HUD able to show
// "HP: 22 / 20".
//
// loadOrGenerateLevel() is private -- it's exercised through the real public entry point
// GameActivity::onEnter() (GameActivity.cpp:98-114), which calls it unconditionally on
// every activity entry. Each trial below constructs a fresh player via the real
// GAME_STATE.newGame(), pins the +maxHp sponsor and hp at its boosted cap (the exact
// precondition from review), then calls the real onEnter() and inspects whatever sponsor
// it actually rolled and whether hp survived the transition still <= the new cap.
//
// Build: see test/sponsor_hp_clamp/README.

#include <HalDisplay.h>
#include <HalGPIO.h>

#include "activities/game/GameActivity.h"

#include "game/GameState.h"
#include "game/GameTypes.h"

#include <cstdio>

namespace {
int failures = 0;
}

#define CHECK(cond, ...)                          \
  do {                                             \
    if (!(cond)) {                                 \
      std::fprintf(stderr, "FAIL: " __VA_ARGS__);  \
      std::fprintf(stderr, "\n");                  \
      failures++;                                  \
    }                                              \
  } while (0)

// Finds the numeric id of the real SPONSOR_DEFS entry with stat==MaxHp and a positive
// amount (the "Big Hollow Insurance"-shaped case at time of writing) -- looked up by shape
// rather than hardcoded index, so a future SPONSOR_DEFS reorder doesn't silently break this.
int findPositiveMaxHpSponsorId() {
  for (int i = 0; i < game::SPONSOR_DEF_COUNT; i++) {
    if (game::SPONSOR_DEFS[i].stat == game::SponsorStat::MaxHp && game::SPONSOR_DEFS[i].amount > 0) return i;
  }
  return -1;
}

void testHpClampedAfterLosingMaxHpSponsorOnFloorLoad() {
  int maxHpSponsorId = findPositiveMaxHpSponsorId();
  CHECK(maxHpSponsorId >= 0, "no positive-amount MaxHp sponsor found in real SPONSOR_DEFS -- test precondition unmet");
  if (maxHpSponsorId < 0) return;

  GfxRenderer renderer(display);
  renderer.begin();
  MappedInputManager input(gpio, renderer);

  constexpr int kTrials = 200;
  int trialsWithSponsorChange = 0;
  int violations = 0;
  uint16_t worstOverage = 0;

  for (int trial = 0; trial < kTrials; trial++) {
    GAME_STATE.newGame(0xF100D + static_cast<uint32_t>(trial));
    auto& p = GAME_STATE.player;

    // Precondition from review: carry the +maxHp sponsor, hp at the boosted cap it grants.
    p.activeSponsorId = static_cast<uint8_t>(maxHpSponsorId);
    uint16_t boostedCap = game::effectiveMaxHp(p);
    if (trial == 0) {
      CHECK(boostedCap > p.maxHp, "chosen sponsor id %d did not raise effectiveMaxHp above base maxHp "
            "(boostedCap=%u, base maxHp=%u) -- test precondition unmet", maxHpSponsorId, boostedCap, p.maxHp);
    }
    p.hp = boostedCap;

    // Real public entry point; internally rolls this floor's sponsor (game::SPONSOR_DEF_COUNT-
    // wide, uniform, real RNG, no seeding trick) and, per the ad370e9 fix, must clamp hp to the
    // resulting effectiveMaxHp before returning.
    GameActivity activity(renderer, input);
    activity.onEnter();

    uint16_t capAfter = game::effectiveMaxHp(p);
    if (p.activeSponsorId != maxHpSponsorId) {
      trialsWithSponsorChange++;
      if (p.hp > capAfter) {
        violations++;
        uint16_t overage = static_cast<uint16_t>(p.hp - capAfter);
        if (overage > worstOverage) worstOverage = overage;
      }
    }
  }

  CHECK(trialsWithSponsorChange > 0, "%d trials, real sponsor roll never once landed on a non-maxHp sponsor -- "
        "test never actually exercised the regression case", kTrials);
  CHECK(violations == 0, "%d of %d sponsor-change trials left hp above effectiveMaxHp after onEnter() "
        "(worst overage %u hp) -- the post-sponsor-roll clamp in loadOrGenerateLevel() did not hold",
        violations, trialsWithSponsorChange, worstOverage);

  std::printf("[sponsor-hp-clamp] %d trials via real GameActivity::onEnter()->loadOrGenerateLevel(), "
              "starting sponsor id %d (%s, +%d maxHp): %d/%d trials rolled a different sponsor, "
              "%d clamp violations (worst overage %u hp)\n",
              kTrials, maxHpSponsorId, game::SPONSOR_DEFS[maxHpSponsorId].name,
              game::SPONSOR_DEFS[maxHpSponsorId].amount, trialsWithSponsorChange, kTrials,
              violations, worstOverage);
}

int main() {
  testHpClampedAfterLosingMaxHpSponsorOnFloorLoad();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
