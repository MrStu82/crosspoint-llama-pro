#pragma once
#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

// Frontlight brightness / warm-cool balance control. All writes go through the
// app-level frontlightManager (Frontlight.h), which wraps the freeink-sdk
// FrontlightManager — never touch LEDC/GPIO here directly.
class FrontlightActivity final : public Activity {
 public:
  explicit FrontlightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Frontlight", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isFrontlightActivity() const override { return true; }

 private:
  ButtonNavigator buttonNavigator;
  OptionPopup optionPopup;

  int selectedIndex = 0;
  // Decided in onEnter() based on frontlightManager.hasColorTemperature() so the
  // warm/cool row is hidden entirely on single-channel boards.
  int visibleItemCount = 0;

  void handleSelection();
  void openBrightnessPicker();
  void openWarmCoolPicker();
  void openPinDiagnostic();
};
