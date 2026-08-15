#include "FlavorText.h"

namespace game {

FlavorTextTracker FlavorTextTracker::instance;

const char* FlavorTextTracker::pick(FlavorCategory category) {
  const auto& set = flavor_detail::kFlavorSets[static_cast<uint8_t>(category)];
  if (set.count == 0) return "";
  if (set.count == 1) {
    lastIndex[static_cast<uint8_t>(category)] = 0;
    return set.variants[0];
  }

  uint8_t idx = static_cast<uint8_t>(GAME_STATE.rollRange(set.count));
  uint8_t last = lastIndex[static_cast<uint8_t>(category)];
  if (idx == last) {
    idx = static_cast<uint8_t>((idx + 1) % set.count);
  }
  lastIndex[static_cast<uint8_t>(category)] = idx;
  return set.variants[idx];
}

}  // namespace game
