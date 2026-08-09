#pragma once

#include "activities/Activity.h"

// Isolates each frontlight LEDC channel at full brightness in turn (GPIO8/cool, then
// GPIO9/warm), clearly labelled on screen, so a single flash+observe cycle tells us
// which pin lights the panel and what colour — distinguishing swapped pins, one dead
// channel, and both-pins-wrong without a guessing round-trip. Restores the user's
// saved brightness/warm-cool settings on exit no matter which phase was reached.
class FrontlightPinDiagnosticActivity final : public Activity {
 public:
  explicit FrontlightPinDiagnosticActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FrontlightPinDiagnostic", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum State { PHASE_COOL, PHASE_WARM, DONE };
  State state = PHASE_COOL;

  void enterPhase(State newState);
  void restoreSavedFrontlight();
};
