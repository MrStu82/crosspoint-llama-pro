#pragma once

#include "activities/Activity.h"

// Glyph title screen shown on entry to Deep Mines. Any button press starts (or resumes) the run
// by handing off to GameActivity via ActivityManager::replaceActivity.
class GameTitleActivity final : public Activity {
  bool rendered = false;

 public:
  explicit GameTitleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameTitle", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
