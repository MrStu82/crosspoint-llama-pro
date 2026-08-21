// Standalone host harness for the real four-tier loot-box functions in GameTypes.h.
// Build: g++ -std=c++20 -O2 -Wall -Wextra -I test/game_save/stubs -I src -I src/game \
//   -I lib/Serialization test/loot_box/LootBoxEligibilityHarness.cpp src/game/GameState.cpp \
//   src/game/FlavorText.cpp src/game/AchievementBus.cpp -o /tmp/lootbox_harness
#include "game/GameState.h"
#include "game/GameTypes.h"
#include <cstdio>

namespace { int failures = 0; uint32_t scriptedRoll = 0; }
#define CHECK(cond, ...) do { if (!(cond)) { std::fprintf(stderr, "FAIL: " __VA_ARGS__); std::fprintf(stderr, "\n"); failures++; } } while (0)
uint32_t scriptedRollRange(uint32_t max) { return max == 0 ? 0 : scriptedRoll % max; }

void testDefinitionLayout() {
  CHECK(game::MASTER_KEY_DEF == game::ITEM_DEF_COUNT - 2, "Master Key must remain second-last");
  CHECK(game::RING_OF_POWER_DEF == game::ITEM_DEF_COUNT - 1, "Ring of Power must remain last");
  CHECK(game::LOOT_BOX_TIER_COUNT == 4, "expected four tiers, got %d", game::LOOT_BOX_TIER_COUNT);
  for (uint8_t i = game::LOOT_BOX_FIRST_DEF; i <= game::LOOT_BOX_LAST_DEF; i++) {
    CHECK(game::ITEM_DEFS[i].type == static_cast<uint8_t>(game::ItemType::LootBox),
          "ItemDef %u in box range is not a LootBox", i);
    CHECK(game::ITEM_DEFS[i].subtype == i - game::LOOT_BOX_FIRST_DEF,
          "ItemDef %u subtype does not match tier offset", i);
  }
}

void testExactTierBoundaries() {
  struct Case { uint32_t roll; game::LootBoxTier expected; } cases[] = {
      {0, game::LootBoxTier::Common}, {49, game::LootBoxTier::Common},
      {50, game::LootBoxTier::Uncommon}, {79, game::LootBoxTier::Uncommon},
      {80, game::LootBoxTier::Rare}, {94, game::LootBoxTier::Rare},
      {95, game::LootBoxTier::Legendary}, {99, game::LootBoxTier::Legendary},
  };
  for (const auto& c : cases) {
    scriptedRoll = c.roll;
    CHECK(game::rollLootBoxTier(scriptedRollRange) == c.expected, "roll %u mapped to wrong tier", c.roll);
  }
  std::printf("[lootbox] exact 50/30/15/5 tier boundaries PASS\n");
}

template <size_t N>
void validateTable(const char* name, const game::LootBoxReward (&table)[N]) {
  CHECK(N > 0, "%s table is empty", name);
  for (const auto& reward : table) {
    switch (reward.kind) {
      case game::LootBoxRewardKind::Item:
        CHECK(reward.id < game::ITEM_DEF_COUNT, "%s invalid ItemDef %u", name, reward.id);
        CHECK(reward.id != game::MASTER_KEY_DEF && reward.id != game::RING_OF_POWER_DEF,
              "%s exposes a quest item", name);
        CHECK(!game::isLootBoxDef(reward.id), "%s can award another box", name);
        break;
      case game::LootBoxRewardKind::Buff:
        CHECK(reward.id > game::BUFF_NONE && reward.id < game::BUFF_DEF_COUNT,
              "%s invalid BuffDef %u", name, reward.id);
        break;
      case game::LootBoxRewardKind::Skill:
        CHECK(reward.id > game::SKILL_NONE && reward.id < game::SKILL_DEF_COUNT,
              "%s invalid SkillDef %u", name, reward.id);
        break;
    }
    CHECK(game::lootBoxRewardName(reward)[0] != '\0', "%s has blank reward copy", name);
  }
}

void testFourRealRewardTables() {
  validateTable("Common", game::COMMON_BOX_REWARDS);
  validateTable("Uncommon", game::UNCOMMON_BOX_REWARDS);
  validateTable("Rare", game::RARE_BOX_REWARDS);
  validateTable("Legendary", game::LEGENDARY_BOX_REWARDS);
  scriptedRoll = 0;
  CHECK(game::selectLootBoxReward(game::LootBoxTier::Common, scriptedRollRange).kind == game::COMMON_BOX_REWARDS[0].kind,
        "Common selection is not linked to Common table");
  CHECK(game::selectLootBoxReward(game::LootBoxTier::Legendary, scriptedRollRange).kind == game::LEGENDARY_BOX_REWARDS[0].kind,
        "Legendary selection is not linked to Legendary table");
  std::printf("[lootbox] four real tables valid; no quest item, nested box, or parallel reward definitions\n");
}

void testSharedLootUsesOneWeightedBoxSlot() {
  game::Rng rng(0xB00BCAFEu);
  constexpr int draws = 200000;
  int boxes[4] = {};
  for (int i = 0; i < draws; i++) {
    const game::Item item = game::rollLootItem(10, rng);
    if (item.type == static_cast<uint8_t>(game::ItemType::LootBox)) {
      CHECK(item.subtype < 4, "shared loot emitted invalid box subtype %u", item.subtype);
      if (item.subtype < 4) boxes[item.subtype]++;
    }
  }
  const int total = boxes[0] + boxes[1] + boxes[2] + boxes[3];
  CHECK(total > 5000 && total < 15000, "box slot frequency wrong: %d/%d", total, draws);
  const int expectedPct[] = {50, 30, 15, 5};
  for (int tier = 0; tier < 4; tier++) {
    const double pct = total == 0 ? 0.0 : 100.0 * boxes[tier] / total;
    CHECK(pct > expectedPct[tier] - 2.0 && pct < expectedPct[tier] + 2.0,
          "tier %d distribution %.2f%% outside %d%% +/-2", tier, pct, expectedPct[tier]);
  }
  std::printf("[lootbox] shared loot boxes=%d/%d; tier counts=%d/%d/%d/%d\n",
              total, draws, boxes[0], boxes[1], boxes[2], boxes[3]);
}

int main() {
  testDefinitionLayout();
  testExactTierBoundaries();
  testFourRealRewardTables();
  testSharedLootUsesOneWeightedBoxSlot();
  if (failures == 0) { std::printf("ALL CHECKS PASSED\n"); return 0; }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
