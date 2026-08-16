#pragma once

#include <cstdint>

#include "GameTypes.h"
#include "SaveValidity.h"

// Handles per-level state persistence.
// Stores deltas from seed-generated state: fog of war, surviving monsters,
// and items remaining/dropped on each level.
class GameSave {
 public:
  // Save current level state to /.crosspoint/game/level_NN.bin
  // doorOpen is a MAP_SIZE-bit bitmap (same layout as fogOfWar): bit set means the door tile
  // at that map index has been opened by the player and should stay open on reload, instead
  // of reverting to DoorClosed when the level is regenerated from the seed.
  static bool saveLevel(uint8_t depth, const uint8_t* fogOfWar, const uint8_t* doorOpen,
                        const game::Monster* monsters, uint8_t monsterCount, const game::Item* items,
                        uint8_t itemCount);

  // Load level state from file. Returns false if no save exists for this depth.
  // doorOpen may be nullptr when loading a file saved before door persistence was added
  // (older LEVEL_FILE_VERSION) — the bitmap is simply left untouched in that case.
  static bool loadLevel(uint8_t depth, uint8_t* fogOfWar, uint8_t* doorOpen, game::Monster* monsters,
                        uint8_t& monsterCount, game::Item* items, uint8_t& itemCount);

  // Check if a saved level file exists
  static bool hasLevel(uint8_t depth);

  // Read-only validity check for level `depth`'s save file -- does it exist,
  // and if so, is it loadable? Internally shares loadLevel()'s exact
  // parse/validate path (see GameSave.cpp), so this can never drift from what
  // loadLevel() actually accepts or rejects. Used by both the load-boundary
  // notification and the Save Data Audit menu scan.
  static SaveValidity validateLevel(uint8_t depth);

  // Delete a specific level file
  static void deleteLevel(uint8_t depth);

  // Delete all game save data (save.bin + all level files)
  static void deleteAll();
};
