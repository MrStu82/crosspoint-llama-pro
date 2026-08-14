#pragma once

#include "activities/Activity.h"

class StatsActivity final : public Activity {
  int countEpubsRecursively(const char* path);

 public:
  explicit StatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Stats", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // Book count is expensive to compute (full recursive SD walk), so it's cached for the
  // boot session rather than recomputed on every StatsActivity open. The only real event
  // that can change it is a USB MSC transfer session -- call this once that ends.
  static void invalidateBookCountCache();
};
