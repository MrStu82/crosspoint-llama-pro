#include <cstdlib>
#include <cstring>
#include <iostream>
#include <algorithm>

#include "DungeonScrollEffects.h"

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
}  // namespace

int main() {
  game::Tile tiles[game::MAP_SIZE];
  uint8_t fog[game::FOG_SIZE];
  std::fill_n(tiles, game::MAP_SIZE, game::Tile::Wall);
  std::memset(fog, 0, sizeof(fog));
  tiles[1 * game::MAP_WIDTH + 1] = game::Tile::Floor;
  tiles[2 * game::MAP_WIDTH + 2] = game::Tile::DoorOpen;
  tiles[3 * game::MAP_WIDTH + 3] = game::Tile::StairsDown;

  require(game::revealWalkableTiles(tiles, fog) == 3, "Mapping must reveal every newly walkable tile");
  require(game::fogIsExplored(fog, 1, 1) && game::fogIsExplored(fog, 2, 2) &&
              game::fogIsExplored(fog, 3, 3),
          "Mapping fog bits missing");
  require(!game::fogIsExplored(fog, 0, 0), "Mapping must not reveal walls");
  require(game::revealWalkableTiles(tiles, fog) == 0, "second Mapping pass must be idempotent");

  game::Monster monsters[1]{};
  monsters[0].x = 2;
  monsters[0].y = 2;
  monsters[0].hp = 1;
  int tx = -1, ty = -1;
  const bool teleported = game::chooseTeleportDestination(
      tiles, monsters, 1, true, 3, 3, 1, 1,
      [](uint32_t max) { return max - 1; }, &tx, &ty);
  require(!teleported, "Teleport must not choose occupied monster/pet/current tiles");

  tiles[4 * game::MAP_WIDTH + 4] = game::Tile::Floor;
  require(game::chooseTeleportDestination(
              tiles, monsters, 1, true, 3, 3, 1, 1,
              [](uint32_t max) { return max - 1; }, &tx, &ty),
          "Teleport must find a safe destination");
  require(tx == 4 && ty == 4, "Teleport selected an unsafe destination");

  std::cout << "Dungeon scroll effects: PASS (Mapping reveals 3/3 walkable; Teleport relocates safely)\n";
  return 0;
}
