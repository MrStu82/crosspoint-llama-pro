#pragma once

#include <array>
#include <cstdint>

namespace TestPwm {
inline std::array<uint32_t, 2> duty = {0, 0};
inline std::array<bool, 2> attached = {false, false};
inline void reset() {
  duty = {0, 0};
  attached = {false, false};
}
}  // namespace TestPwm

inline double ledcSetup(uint8_t, double frequency, uint8_t) { return frequency; }
inline bool ledcAttachPin(uint8_t, uint8_t channel) {
  TestPwm::attached.at(channel) = true;
  return true;
}
inline void ledcWrite(uint8_t channel, uint32_t duty) { TestPwm::duty.at(channel) = duty; }
