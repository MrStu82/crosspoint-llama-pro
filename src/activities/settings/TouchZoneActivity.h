#pragma once

#if FREEINK_DEVICE_PICCO
#include "activities/Activity.h"

// Settings > Controls > Touch Zone (Picco only): the reader's 3x3 tap grid.
// Tap a cell to cycle its action; saved on each change, Back to leave.
class TouchZoneActivity final : public Activity {
 public:
  explicit TouchZoneActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TouchZone", renderer, mappedInput) {}

  void loop() override;
  void render(RenderLock&&) override;

 private:
  int gridTop() const;
};
#endif
