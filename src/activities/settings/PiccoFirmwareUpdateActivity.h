#pragma once
#if FREEINK_DEVICE_PICCO

#include <string>

#include "activities/Activity.h"

// Picco-only: checks /update/firmware.bin, then hands it to the stock safeboot
// installer (markers + boot factory + restart). Nothing is written on a failed check.
class PiccoFirmwareUpdateActivity final : public Activity {
 public:
  explicit PiccoFirmwareUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PiccoFirmwareUpdate", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::CHECKING; }

 private:
  enum class State { READY, CHECKING, FAILED, RESTARTING };
  State state = State::READY;
  bool fileFound = false;
  std::string reason;

  void startCheck();
  void onConfirmationResult(const ActivityResult& result);
  void fail(const char* why);
  const char* handOffToSafeboot();
};

#endif
