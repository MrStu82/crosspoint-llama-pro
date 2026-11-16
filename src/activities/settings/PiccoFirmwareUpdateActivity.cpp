#if FREEINK_DEVICE_PICCO
#include "PiccoFirmwareUpdateActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "MappedInputManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/PiccoUpdate.h"

namespace {
constexpr int MIN_BATTERY_PERCENT = 30;  // safeboot defers the install at <=29% without USB

bool writeEmptyMarker(const char* path) {
  HalFile file;
  if (!Storage.openFileForWrite("PUPD", path, file) || !file) return false;
  file.flush();
  file.close();
  return Storage.exists(path);
}

void removeMarkers() {
  Storage.remove(picco_update::OTA_PENDING_PATH);
  Storage.remove(picco_update::FACTORY_RLOOP_PATH);
}
}  // namespace

void PiccoFirmwareUpdateActivity::onEnter() {
  Activity::onEnter();
  Storage.ensureDirectoryExists("/update");  // gives the Wi-Fi file page an upload target
  fileFound = Storage.exists(picco_update::FIRMWARE_PATH);
  state = State::READY;
  requestUpdate();
}

void PiccoFirmwareUpdateActivity::fail(const char* why) {
  LOG_ERR("PUPD", "update refused: %s", why);
  RenderLock lock(*this);
  reason = why;
  state = State::FAILED;
  requestUpdate();
}

void PiccoFirmwareUpdateActivity::startCheck() {
  {
    RenderLock lock(*this);
    state = State::CHECKING;
  }
  requestUpdateAndWait();
  if (const char* why = picco_update::preflight(picco_update::FIRMWARE_PATH)) {
    fail(why);
    return;
  }
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, "Install update?", "Restarts into recovery"),
      [this](const ActivityResult& result) { onConfirmationResult(result); });
}

void PiccoFirmwareUpdateActivity::onConfirmationResult(const ActivityResult& result) {
  if (result.isCancelled) {
    RenderLock lock(*this);
    state = State::READY;
    requestUpdate();
    return;
  }
  if (const char* why = handOffToSafeboot()) {
    fail(why);
    return;
  }
  {
    RenderLock lock(*this);
    state = State::RESTARTING;
  }
  requestUpdateAndWait();
  LOG_INF("PUPD", "handing off to safeboot");
  delay(500);
  esp_restart();
}

// Returns nullptr once safeboot is selected; otherwise the reason, with nothing left written.
const char* PiccoFirmwareUpdateActivity::handOffToSafeboot() {
  if (!gpio.isUsbConnected() && powerManager.getBatteryPercentage() < MIN_BATTERY_PERCENT) {
    return "Charge above 29% or plug in USB";
  }
  const esp_partition_t* factory =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (!factory) return "No recovery partition";
  if (!writeEmptyMarker(picco_update::OTA_PENDING_PATH) || !writeEmptyMarker(picco_update::FACTORY_RLOOP_PATH)) {
    removeMarkers();
    return "Could not write update markers";
  }
  if (esp_ota_set_boot_partition(factory) != ESP_OK) {
    removeMarkers();
    return "Could not select recovery";
  }
  return nullptr;
}

void PiccoFirmwareUpdateActivity::loop() {
  if (state == State::CHECKING || state == State::RESTARTING) return;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  const bool activated =
      mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y);
  if (!activated) return;
  if (state == State::FAILED) {
    finish();
    return;
  }
  if (fileFound) startCheck();
}

void PiccoFirmwareUpdateActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, "Firmware update");

  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  renderer.drawCenteredText(UI_12_FONT_ID, y, "Running version");
  y += lineHeight;
  renderer.drawCenteredText(UI_12_FONT_ID, y, CROSSPOINT_VERSION, true, EpdFontFamily::BOLD);
  y += lineHeight + metrics.verticalSpacing * 2;

  const char* body = nullptr;
  switch (state) {
    case State::READY:
      body = fileFound ? "/update/firmware.bin found. Tap to check and install."
                       : "No update. Upload firmware.bin to the /update folder with Wi-Fi file transfer.";
      break;
    case State::CHECKING:
      body = "Checking firmware...";
      break;
    case State::FAILED:
      body = reason.c_str();
      break;
    case State::RESTARTING:
      body = "Restarting into recovery to install. Do not power off.";
      break;
  }
  if (state == State::FAILED) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, "Update not started", true, EpdFontFamily::BOLD);
    y += lineHeight + metrics.verticalSpacing;
  }
  const Rect bounds{metrics.contentSidePadding, y, width - metrics.contentSidePadding * 2, height - y};
  UITheme::drawCenteredWrappedText(renderer, bounds, UI_12_FONT_ID, body, 4, true, EpdFontFamily::REGULAR,
                                   UITheme::TextVerticalAlignment::TOP);
  renderer.displayBuffer();
}
#endif
