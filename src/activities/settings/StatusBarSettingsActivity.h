#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "CrossPointSettings.h"
#include "util/ButtonNavigator.h"

// Reader status bar configuration activity. Shared by both bars: `edge`
// selects which mirrored field set (statusBar* vs topBar*) is read/written.
// The top bar has no clock/XTC entries (those are bottom-only concepts), so
// its menu is a shorter prefix of the same item list.
class StatusBarSettingsActivity final : public Activity {
 public:
  explicit StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     CrossPointSettings::Edge edge = CrossPointSettings::Edge::BOTTOM)
      : Activity("StatusBarSettings", renderer, mappedInput), edge(edge) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  CrossPointSettings::Edge edge;
  ButtonNavigator buttonNavigator;
  OptionPopup optionPopup;

  int selectedIndex = 0;
  // Decided in onEnter() based on edge and (for the bottom bar) halClock.isAvailable()
  // so clock entries are hidden on X4 and always hidden on the top bar.
  int visibleItemCount = 0;

  bool isTop() const { return edge == CrossPointSettings::Edge::TOP; }
  void handleSelection();
};
