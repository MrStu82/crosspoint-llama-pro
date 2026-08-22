#pragma once

#include <cstddef>
#include <cstdint>

#include "GameTypes.h"

namespace game {

inline bool isDungeonWalkable(Tile tile) {
  return tile == Tile::Floor || tile == Tile::DoorOpen || tile == Tile::StairsUp ||
         tile == Tile::StairsDown || tile == Tile::Rubble;
}

inline size_t revealWalkableTiles(const Tile* tiles, uint8_t* fog) {
  size_t revealed = 0;
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      if (!isDungeonWalkable(tiles[y * MAP_WIDTH + x])) continue;
      if (!fogIsExplored(fog, x, y)) revealed++;
      fogSetExplored(fog, x, y);
    }
  }
  return revealed;
}

template <typename RollRange>
bool chooseTeleportDestination(const Tile* tiles, const Monster* monsters, uint8_t monsterCount,
                               bool petActive, int petX, int petY, int currentX, int currentY,
                               RollRange&& rollRange, int* outX, int* outY) {
  uint32_t candidates = 0;
  int chosenX = currentX;
  int chosenY = currentY;

  // Reservoir sampling avoids a MAP_SIZE scratch array while keeping every
  // valid destination equally likely.
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      if (x == currentX && y == currentY) continue;
      if (!isDungeonWalkable(tiles[y * MAP_WIDTH + x])) continue;
      if (petActive && x == petX && y == petY) continue;

      bool occupied = false;
      for (uint8_t i = 0; i < monsterCount; i++) {
        if (monsters[i].hp > 0 && monsters[i].x == x && monsters[i].y == y) {
          occupied = true;
          break;
        }
      }
      if (occupied) continue;

      candidates++;
      if (rollRange(candidates) == 0) {
        chosenX = x;
        chosenY = y;
      }
    }
  }

  if (candidates == 0) return false;
  *outX = chosenX;
  *outY = chosenY;
  return true;
}

}  // namespace game
