#include "GameTypes.h"

#include <cstdio>
#include <cstring>

namespace {
int failures = 0;

#define CHECK(condition, message) \
  do {                            \
    if (!(condition)) {           \
      std::printf("FAIL: %s\n", message); \
      failures++;                 \
    }                             \
  } while (0)

game::Item makeItem(game::ItemType type, uint8_t subtype, uint8_t marker, uint8_t flags = 0) {
  game::Item item{};
  item.type = static_cast<uint8_t>(type);
  item.subtype = subtype;
  item.enchantment = marker;
  item.count = 1;
  item.flags = flags;
  return item;
}
}  // namespace

int main() {
  game::Item inventory[] = {
      makeItem(game::ItemType::Food, 0, 1),
      makeItem(game::ItemType::Weapon, 1, 2),
      makeItem(game::ItemType::Potion, 0, 3),
      makeItem(game::ItemType::Weapon, 0, 4),
      makeItem(game::ItemType::Armor, 0, 5),
  };

  game::sortInventoryByType(inventory, 5);
  CHECK(inventory[0].type == static_cast<uint8_t>(game::ItemType::Weapon), "weapon group first");
  CHECK(inventory[1].type == static_cast<uint8_t>(game::ItemType::Weapon), "second weapon remains grouped");
  CHECK(inventory[0].enchantment == 2 && inventory[1].enchantment == 4,
        "same-type pickup order stays stable");
  CHECK(inventory[2].type == static_cast<uint8_t>(game::ItemType::Armor), "armor follows weapons");
  CHECK(inventory[3].type == static_cast<uint8_t>(game::ItemType::Potion), "consumables grouped by type");
  CHECK(inventory[4].type == static_cast<uint8_t>(game::ItemType::Food), "food group retained");

  game::Item sword = makeItem(game::ItemType::Weapon, 1, 0);
  sword.count = 3;
  CHECK(game::isEquippable(sword), "weapon exposes Equip/Unequip");
  CHECK(!game::isUsable(sword), "weapon does not expose Use");
  CHECK(game::itemSaleValue(sword) == 45, "Sell amount is definition value times stack count");

  game::Item potion = makeItem(game::ItemType::Potion, 0, 0);
  CHECK(game::isUsable(potion), "potion exposes Use");
  CHECK(!game::isEquippable(potion), "potion does not expose Equip");

  inventory[0].flags = static_cast<uint8_t>(game::ItemFlag::New) |
                       static_cast<uint8_t>(game::ItemFlag::Equipped);
  inventory[1].flags = static_cast<uint8_t>(game::ItemFlag::New) |
                       static_cast<uint8_t>(game::ItemFlag::Identified);
  game::clearNewInventoryFlags(inventory, 5);
  CHECK((inventory[0].flags & static_cast<uint8_t>(game::ItemFlag::New)) == 0, "first New flag clears");
  CHECK((inventory[1].flags & static_cast<uint8_t>(game::ItemFlag::New)) == 0, "second New flag clears");
  CHECK((inventory[0].flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) != 0,
        "clearing New preserves Equipped");
  CHECK((inventory[1].flags & static_cast<uint8_t>(game::ItemFlag::Identified)) != 0,
        "clearing New preserves Identified");

  std::printf("Inventory feature harness: %s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
              failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
