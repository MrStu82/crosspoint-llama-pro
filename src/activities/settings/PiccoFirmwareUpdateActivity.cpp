#if FREEINK_DEVICE_PICCO
#include "PiccoFirmwareUpdateActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MIN_BATTERY_PERCENT = 30;  // safeboot defers the install at <=29% without USB
constexpr int BUTTON_HEIGHT = 66;
constexpr int BUTTON_GAP = 20;
constexpr int BUTTON_BOTTOM_MARGIN = 40;

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
  setState(State::READY);
}

void PiccoFirmwareUpdateActivity::onExit() {
  Activity::onExit();
  if (tearDownWifi) {
    WiFi.disconnect(false);
    delay(30);
    silentRestartToSettings();  // clears the heap the Wi-Fi session fragmented
  }
}

void PiccoFirmwareUpdateActivity::setState(const State next) {
  {
    RenderLock lock(*this);
    if (next == State::READY) fileFound = Storage.exists(picco_update::FIRMWARE_PATH);
    state = next;
  }
  requestUpdate();
}

void PiccoFirmwareUpdateActivity::fail(const char* why) {
  LOG_ERR("PUPD", "update refused: %s", why);
  {
    RenderLock lock(*this);
    reason = why;
  }
  setState(State::FAILED);
}

void PiccoFirmwareUpdateActivity::checkForUpdate() {
  if (WiFi.status() == WL_CONNECTED) {
    setState(State::FETCHING);
    return;
  }
  tearDownWifi = true;
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           setState(result.isCancelled ? State::READY : State::FETCHING);
                         });
}

void PiccoFirmwareUpdateActivity::runFetch() {
  requestUpdateAndWait();
  if (WiFi.status() != WL_CONNECTED) {
    fail("Wi-Fi not connected");
    return;
  }
  picco_update::Manifest fetched;
  if (const char* why = picco_update::fetchManifest(fetched)) {
    fail(why);
    return;
  }
  {
    RenderLock lock(*this);
    manifest = fetched;
  }
  setState(manifest.version == CROSSPOINT_VERSION ? State::UP_TO_DATE : State::AVAILABLE);
}

void PiccoFirmwareUpdateActivity::runDownload() {
  requestUpdateAndWait();
  if (const char* why = picco_update::download(manifest)) {
    fail(why);
    return;
  }
  // The pulled file now takes the same path as a browser upload, minus the second confirm.
  if (const char* why = picco_update::preflight(picco_update::FIRMWARE_PATH)) {
    Storage.remove(picco_update::FIRMWARE_PATH);
    fail(why);
    return;
  }
  install();
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
    setState(State::READY);
    return;
  }
  install();
}

void PiccoFirmwareUpdateActivity::install() {
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

int PiccoFirmwareUpdateActivity::buttonCount() const {
  switch (state) {
    case State::READY:
      return fileFound ? 2 : 1;
    case State::AVAILABLE:
      return 2;
    default:
      return 0;
  }
}

const char* PiccoFirmwareUpdateActivity::buttonLabel(const int i) const {
  if (state == State::AVAILABLE) return i == 0 ? "Download and install" : "Cancel";
  return i == 0 ? "Check for update" : "Install from SD card";
}

void PiccoFirmwareUpdateActivity::onButton(const int i) {
  if (state == State::READY) {
    if (i == 0) {
      checkForUpdate();
    } else {
      startCheck();
    }
  } else if (state == State::AVAILABLE) {
    setState(i == 0 ? State::DOWNLOADING : State::READY);
  }
}

namespace {
Rect buttonRect(const GfxRenderer& renderer, const int count, const int i) {
  const int side = UITheme::getInstance().getMetrics().contentSidePadding;
  const int y = renderer.getScreenHeight() - BUTTON_BOTTOM_MARGIN - (count - i) * (BUTTON_HEIGHT + BUTTON_GAP);
  return Rect{side, y, renderer.getScreenWidth() - side * 2, BUTTON_HEIGHT};
}
}  // namespace

void PiccoFirmwareUpdateActivity::loop() {
  if (state == State::FETCHING) {
    runFetch();
    return;
  }
  if (state == State::DOWNLOADING) {
    runDownload();
    return;
  }
  if (busy()) return;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int x = 0;
  int y = 0;
  const bool tapped = mappedInput.wasScreenTapped(x, y);
  const bool confirmed = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
  if (!tapped && !confirmed) return;
  const int count = buttonCount();
  if (count == 0) {
    setState(State::READY);  // result screens: any tap returns
    return;
  }
  if (confirmed) {
    onButton(0);
    return;
  }
  for (int i = 0; i < count; ++i) {
    const Rect r = buttonRect(renderer, count, i);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) {
      onButton(i);
      return;
    }
  }
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

  std::string body;
  switch (state) {
    case State::READY:
      body = fileFound ? "/update/firmware.bin is on the SD card."
                       : "Check online, or upload firmware.bin to the /update folder with Wi-Fi file transfer.";
      break;
    case State::FETCHING:
      body = "Checking for update...";
      break;
    case State::AVAILABLE:
      body = "New version " + manifest.version + " (" + std::to_string(manifest.size / 1024) + " KB)";
      break;
    case State::UP_TO_DATE:
      body = "Up to date";
      break;
    case State::DOWNLOADING:
      body = "Downloading " + manifest.version + "... Do not power off.";
      break;
    case State::CHECKING:
      body = "Checking firmware...";
      break;
    case State::FAILED:
      body = reason;
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
  UITheme::drawCenteredWrappedText(renderer, bounds, UI_12_FONT_ID, body.c_str(), 4, true, EpdFontFamily::REGULAR,
                                   UITheme::TextVerticalAlignment::TOP);

  const int count = buttonCount();
  for (int i = 0; i < count; ++i) {
    const Rect r = buttonRect(renderer, count, i);
    const char* label = buttonLabel(i);
    renderer.fillRoundedRect(r.x, r.y, r.width, r.height, 10, Color::Black);
    const int labelWidth = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, r.x + (r.width - labelWidth) / 2, r.y + (r.height - lineHeight) / 2, label, false,
                      EpdFontFamily::BOLD);
  }
  renderer.displayBuffer();
}
#endif
