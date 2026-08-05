#pragma once

#include "activities/Activity.h"

class StatsActivity final : public Activity {
  int totalBooksOnDevice = 0;

  int countEpubsRecursively(const char* path);

 public:
  explicit StatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Stats", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
