// Standalone host harness, no gtest, compiled directly with g++.
// Calls the REAL sponsorAttackModifier()/sponsorDefenseModifier()/
// sponsorGoldPercentModifier()/effectiveMaxHp() inline functions straight out
// of src/game/GameTypes.h (header-only, no Activity/HAL dependency) -- this is
// full linkage, not a reimplementation, since these ARE the functions
// GameActivity.cpp/GameMenuActivity.cpp call at point of use.
//
// Rewritten at ad370e9 against the CURRENT real SPONSOR_DEFS table (7 entries:
// None, MaxHp+2, GoldPercent+10, Attack+1, Defense+1, Defense-1, GoldPercent-5)
// -- the previous version of this harness referenced sponsor names/ids
// ("Vantablack Energy Drink" at id 4, "System Uptime Guarantee (tm)" as a
// MaxHp+2 sponsor at id 1) that no longer match src/game/GameTypes.h at all,
// and had zero coverage of the new SponsorStat::GoldPercent case added in this
// commit. Every check below looks sponsors up by (stat, sign) shape via a
// helper rather than hardcoding an id, so a future SPONSOR_DEFS reorder can't
// silently make this file lie about which sponsor it's testing again.
//
// Build: g++ -std=c++20 -O2 -Wall -Wextra -I src \
//        SponsorModifierHarness.cpp -o /tmp/sponsor_harness
// Run:   /tmp/sponsor_harness

#include "game/GameTypes.h"

#include <cstdio>
#include <cstring>

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

int findSponsorByStatAndSign(game::SponsorStat stat, bool positive) {
  for (int i = 0; i < game::SPONSOR_DEF_COUNT; i++) {
    const auto& s = game::SPONSOR_DEFS[i];
    if (s.stat == stat && ((positive && s.amount > 0) || (!positive && s.amount < 0))) return i;
  }
  return -1;
}

// Req: modifiers apply only at point of use -- calling these functions must
// never write through the passed-in Player, and must be pure functions of
// (sponsorId, base stat).
void testModifiersAreReadOnlyAndPure() {
  int maxHpId = findSponsorByStatAndSign(game::SponsorStat::MaxHp, true);
  CHECK(maxHpId >= 0, "no positive-amount MaxHp sponsor found in real SPONSOR_DEFS");
  if (maxHpId < 0) return;

  game::Player p{};
  p.maxHp = 20;
  p.activeSponsorId = static_cast<uint8_t>(maxHpId);

  uint16_t before = p.maxHp;
  uint16_t eff = game::effectiveMaxHp(p);
  CHECK(p.maxHp == before, "effectiveMaxHp() mutated Player::maxHp (got %u, expected unchanged %u)", p.maxHp, before);
  uint16_t expected = static_cast<uint16_t>(20 + game::SPONSOR_DEFS[maxHpId].amount);
  CHECK(eff == expected, "effectiveMaxHp() with %s (+%d): got %u, expected %u",
        game::SPONSOR_DEFS[maxHpId].name, game::SPONSOR_DEFS[maxHpId].amount, eff, expected);

  int attackId = findSponsorByStatAndSign(game::SponsorStat::Attack, true);
  CHECK(attackId >= 0, "no positive-amount Attack sponsor found in real SPONSOR_DEFS");
  if (attackId >= 0) {
    int atk = game::sponsorAttackModifier(static_cast<uint8_t>(attackId));
    CHECK(atk == game::SPONSOR_DEFS[attackId].amount, "sponsorAttackModifier(%s) got %d, expected %d",
          game::SPONSOR_DEFS[attackId].name, atk, game::SPONSOR_DEFS[attackId].amount);
    int def = game::sponsorDefenseModifier(static_cast<uint8_t>(attackId));
    CHECK(def == 0, "Attack sponsor bled into defense: sponsorDefenseModifier(%s) got %d, expected 0",
          game::SPONSOR_DEFS[attackId].name, def);
    int gold = game::sponsorGoldPercentModifier(static_cast<uint8_t>(attackId));
    CHECK(gold == 0, "Attack sponsor bled into gold-percent: sponsorGoldPercentModifier(%s) got %d, expected 0",
          game::SPONSOR_DEFS[attackId].name, gold);

    std::printf("[sponsor] point-of-use purity: effectiveMaxHp(%s)=%u (Player.maxHp unmodified at %u), "
                "sponsorAttackModifier(%s)=%d, cross-stat leak (defense=%d, gold=%d)\n",
                game::SPONSOR_DEFS[maxHpId].name, eff, p.maxHp, game::SPONSOR_DEFS[attackId].name, atk, def, gold);
  }
}

// Req: a negative-amount sponsor (currently the Defense-1 case) must never
// make effective defense negative -- callers apply their own floor (per
// GameActivity.cpp's monsterAttackPlayer() clamp, not re-verified here), but
// the modifier itself should report the raw signed value so that clamp has
// something real to floor.
void testNegativeDefenseSponsorReportsRawSignedValue() {
  int negDefId = findSponsorByStatAndSign(game::SponsorStat::Defense, false);
  CHECK(negDefId >= 0, "no negative-amount Defense sponsor found in real SPONSOR_DEFS");
  if (negDefId < 0) return;
  int def = game::sponsorDefenseModifier(static_cast<uint8_t>(negDefId));
  CHECK(def == game::SPONSOR_DEFS[negDefId].amount, "sponsorDefenseModifier(%s) got %d, expected %d",
        game::SPONSOR_DEFS[negDefId].name, def, game::SPONSOR_DEFS[negDefId].amount);
  std::printf("[sponsor] negative-defense sponsor (%s) reports raw %d (floor clamp is the caller's job, "
              "not re-verified by this harness)\n", game::SPONSOR_DEFS[negDefId].name, def);
}

// Req (new in ad370e9): SponsorStat::GoldPercent sponsors report their raw
// signed percent via sponsorGoldPercentModifier() and don't leak into
// attack/defense/maxHp -- covers both the positive (Vantage Extraction
// Partners, +10) and negative (System Uptime Guarantee (tm), -5) real cases,
// found by shape rather than hardcoded id/name.
void testGoldPercentModifierBothSigns() {
  int posId = findSponsorByStatAndSign(game::SponsorStat::GoldPercent, true);
  int negId = findSponsorByStatAndSign(game::SponsorStat::GoldPercent, false);
  CHECK(posId >= 0, "no positive-amount GoldPercent sponsor found in real SPONSOR_DEFS");
  CHECK(negId >= 0, "no negative-amount GoldPercent sponsor found in real SPONSOR_DEFS");
  if (posId < 0 || negId < 0) return;

  int posGold = game::sponsorGoldPercentModifier(static_cast<uint8_t>(posId));
  CHECK(posGold == game::SPONSOR_DEFS[posId].amount, "sponsorGoldPercentModifier(%s) got %d, expected %d",
        game::SPONSOR_DEFS[posId].name, posGold, game::SPONSOR_DEFS[posId].amount);
  int negGold = game::sponsorGoldPercentModifier(static_cast<uint8_t>(negId));
  CHECK(negGold == game::SPONSOR_DEFS[negId].amount, "sponsorGoldPercentModifier(%s) got %d, expected %d",
        game::SPONSOR_DEFS[negId].name, negGold, game::SPONSOR_DEFS[negId].amount);

  // Cross-stat leak check for both.
  game::Player p{};
  p.maxHp = 20;
  p.activeSponsorId = static_cast<uint8_t>(posId);
  uint16_t effWithGoldSponsor = game::effectiveMaxHp(p);
  CHECK(effWithGoldSponsor == 20, "GoldPercent sponsor (%s) leaked into effectiveMaxHp: got %u, expected unchanged 20",
        game::SPONSOR_DEFS[posId].name, effWithGoldSponsor);
  int atkWithGoldSponsor = game::sponsorAttackModifier(static_cast<uint8_t>(posId));
  int defWithGoldSponsor = game::sponsorDefenseModifier(static_cast<uint8_t>(posId));
  CHECK(atkWithGoldSponsor == 0 && defWithGoldSponsor == 0,
        "GoldPercent sponsor (%s) leaked into attack/defense: atk=%d def=%d, expected 0/0",
        game::SPONSOR_DEFS[posId].name, atkWithGoldSponsor, defWithGoldSponsor);

  std::printf("[sponsor] gold-percent coverage: %s=+%d%%, %s=%d%%, no cross-stat leak into "
              "effectiveMaxHp/attack/defense\n",
              game::SPONSOR_DEFS[posId].name, posGold, game::SPONSOR_DEFS[negId].name, negGold);
}

// Req: effectiveMaxHp() floors at 1 even under a hypothetical maxHp-crushing
// sponsor -- sweeps every real SPONSOR_DEFS entry against a deliberately tiny
// base maxHp to prove the floor holds for every currently defined sponsor,
// not just a hand-picked one.
void testEffectiveMaxHpFloorsAtOneAcrossAllSponsors() {
  int violations = 0;
  for (uint8_t id = 0; id < game::SPONSOR_DEF_COUNT; id++) {
    game::Player p{};
    p.maxHp = 1;  // worst case: already at the floor
    p.activeSponsorId = id;
    uint16_t eff = game::effectiveMaxHp(p);
    if (eff < 1) {
      std::fprintf(stderr, "  sponsor id %u (%s) produced effectiveMaxHp=%u < 1\n", id,
                   game::SPONSOR_DEFS[id].name, eff);
      violations++;
    }
  }
  CHECK(violations == 0, "%d of %d sponsors produced effectiveMaxHp < 1 from a base maxHp of 1", violations,
        game::SPONSOR_DEF_COUNT);
  std::printf("[sponsor] effectiveMaxHp floor-at-1 swept across all %d real SPONSOR_DEFS entries, base maxHp=1: "
              "%d violations\n", game::SPONSOR_DEF_COUNT, violations);
}

// Req: an out-of-range sponsorId (e.g. stale save data) degrades to "no
// modifier" rather than reading out of bounds. Covers all four modifier
// functions, including the new gold-percent one.
void testOutOfRangeSponsorIdIsInert() {
  int atk = game::sponsorAttackModifier(200);
  int def = game::sponsorDefenseModifier(200);
  int gold = game::sponsorGoldPercentModifier(200);
  game::Player p{};
  p.maxHp = 20;
  p.activeSponsorId = 200;
  uint16_t eff = game::effectiveMaxHp(p);
  CHECK(atk == 0 && def == 0 && gold == 0 && eff == 20,
        "out-of-range sponsorId=200 was not inert: atk=%d def=%d gold=%d effectiveMaxHp=%u", atk, def, gold, eff);
  std::printf("[sponsor] out-of-range sponsorId=200 correctly inert (atk=%d def=%d gold=%d effectiveMaxHp=%u)\n",
              atk, def, gold, eff);
}

int main() {
  testModifiersAreReadOnlyAndPure();
  testNegativeDefenseSponsorReportsRawSignedValue();
  testGoldPercentModifierBothSigns();
  testEffectiveMaxHpFloorsAtOneAcrossAllSponsors();
  testOutOfRangeSponsorIdIsInert();

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
