#pragma once
#if FREEINK_DEVICE_PICCO

#include <atomic>
#include <string>

#include "activities/Activity.h"
#include "network/PiccoUpdate.h"

// Picco-only: "Check for update" pulls the manifest firmware to /update/firmware.bin,
// or a browser upload stages it; either way it is checked, then handed to the stock
// safeboot installer (markers + boot factory + restart). Nothing is written on a failure.
class PiccoFirmwareUpdateActivity final : public Activity {
 public:
  explicit PiccoFirmwareUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PiccoFirmwareUpdate", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return busy(); }

 private:
  enum class State { READY, FETCHING, AVAILABLE, UP_TO_DATE, DOWNLOADING, CHECKING, FAILED, RESTARTING };
  State state = State::READY;
  bool fileFound = false;
  bool tearDownWifi = false;
  std::string reason;
  picco_update::Manifest manifest;
  std::atomic<int> downloadStep{0};  // 0..10 tenths received; written by the download, read by render

  static void onDownloadProgress(void* ctx, size_t received);

  bool busy() const {
    return state == State::FETCHING || state == State::DOWNLOADING || state == State::CHECKING ||
           state == State::RESTARTING;
  }
  int buttonCount() const;
  const char* buttonLabel(int i) const;
  void onButton(int i);
  void setState(State next);
  void checkForUpdate();
  void runFetch();
  void runDownload();
  void startCheck();
  void install();
  void onConfirmationResult(const ActivityResult& result);
  void fail(const char* why);
  const char* handOffToSafeboot();
};

#endif
