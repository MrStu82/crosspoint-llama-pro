#pragma once

namespace game {

// X4 Pro's UC8179 implements displayWindow() with the panel driver's full
// FAST waveform. An Action tap always produces a game-frame refresh, so an
// immediate pressed/released refresh is redundant; Menu has no equivalent
// game frame and keeps its feedback. Kept pure so the target behavior is
// pinned by a host harness without dragging in the HAL.
constexpr bool refreshControlFeedbackImmediately(int buttonIndex) { return buttonIndex == 1; }

}  // namespace game
