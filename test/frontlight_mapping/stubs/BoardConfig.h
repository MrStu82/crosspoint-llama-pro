#pragma once

#include <cstdint>

#define FREEINK_CAP_FRONTLIGHT 1

namespace BoardConfig {

constexpr int8_t PIN_UNASSIGNED = -1;

enum class Board : uint8_t { Other, XteinkX4Pro };

struct FrontlightConfig {
  int8_t gpio;
  uint32_t pwmFrequency;
  uint8_t pwmResolutionBits;
  bool activeHigh;
  int8_t gpioWarm;
};

struct BoardProfile {
  Board board;
  FrontlightConfig frontlight;
};

inline BoardProfile ACTIVE = {Board::XteinkX4Pro, {8, 10000, 10, true, 9}};
inline bool isX4Pro() { return ACTIVE.board == Board::XteinkX4Pro; }

}  // namespace BoardConfig
