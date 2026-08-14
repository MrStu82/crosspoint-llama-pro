#pragma once

#include "activities/Activity.h"

// Strictly read-only hardware/firmware facts screen for Settings > Debug Info.
// No toggles, no actions beyond Back — exists so the display controller
// question (which chip, and how it was resolved) and other build facts are
// answerable on-device without a working serial line.
class DebugPanelActivity final : public Activity {
 public:
  explicit DebugPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DebugPanel", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
