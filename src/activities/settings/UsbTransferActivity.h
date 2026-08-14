#pragma once

#include "activities/Activity.h"
#include "UsbMassStorage.h"

/**
 * USB Mass Storage ("USB Transfer") activity.
 *
 * Flow:
 *  1) onEnter -> grab the raw SD block device, UsbMassStorage::begin().
 *  2) loop() polls hostConnected() each frame: WAITING -> CONNECTED once a
 *     host mounts the disk; CONNECTED -> back to finish() once the host
 *     unmounts/unplugs (end() + remount the app FS first).
 *  3) Back button at any point ends the session cleanly (end() + remount).
 *
 * Only reachable when BoardConfig::hasUsbMassStorage() is true.
 */
class UsbTransferActivity : public Activity {
 public:
  enum class State {
    WAITING,    // MSC active, advertising, no host mounted yet
    CONNECTED,  // host has mounted the disk
    ERROR,      // could not start (no SD card / begin() failed)
  };

  explicit UsbTransferActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("UsbTransfer", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state != State::ERROR; }

 private:
  State state = State::WAITING;
  bool wasConnected = false;
  freeink::UsbMassStorage usbMsc;

  void endSessionAndFinish();
};
