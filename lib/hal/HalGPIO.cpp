#include <HalGPIO.h>
#include <Logging.h>
#include <PowerManager.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <XteinkDetect.h>
#include <esp_sleep.h>

#include "GpioWakeUsbPolicy.h"

// Global HalGPIO instance
HalGPIO gpio;

namespace X3GPIO {

bool readI2CReg16LE(uint8_t addr, uint8_t reg, uint16_t* outValue) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(addr, static_cast<uint8_t>(2), static_cast<uint8_t>(true)) < 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *outValue = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}

bool readBQ27220CurrentMA(int16_t* outCurrent) {
  uint16_t raw = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_CUR_REG, &raw)) {
    return false;
  }
  *outCurrent = static_cast<int16_t>(raw);
  return true;
}

}  // namespace X3GPIO

namespace {
// Set by applyDisplayControllerWithOverride() each boot, mirrors the "source"
// half of its own LOG_INF line — kept so the DEBUG panel can display how the
// controller was resolved without re-probing the display bus at render time.
const char* g_displayControllerSource = "unknown";

constexpr char HW_NAMESPACE[] = "cphw";
constexpr char NVS_KEY_DEV_OVERRIDE[] = "dev_ovr";  // 0=auto, 1=x4, 2=x3
constexpr char NVS_KEY_DEV_CACHED[] = "dev_det";    // 0=unknown, 1=x4, 2=x3

enum class NvsDeviceValue : uint8_t { Unknown = 0, X4 = 1, X3 = 2 };

NvsDeviceValue readNvsDeviceValue(const char* key, NvsDeviceValue defaultValue) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) {
    return defaultValue;
  }
  const uint8_t raw = prefs.getUChar(key, static_cast<uint8_t>(defaultValue));
  prefs.end();
  if (raw > static_cast<uint8_t>(NvsDeviceValue::X3)) {
    return defaultValue;
  }
  return static_cast<NvsDeviceValue>(raw);
}

void writeNvsDeviceValue(const char* key, NvsDeviceValue value) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) {
    return;
  }
  prefs.putUChar(key, static_cast<uint8_t>(value));
  prefs.end();
}

HalGPIO::DeviceType nvsToDeviceType(NvsDeviceValue value) {
  return value == NvsDeviceValue::X3 ? HalGPIO::DeviceType::X3 : HalGPIO::DeviceType::X4;
}

constexpr char NVS_KEY_DISP_OVERRIDE[] = "disp_ovr";  // 0=auto, 1=force SSD1677, 2=force UC8179

enum class DisplayControllerOverride : uint8_t { Auto = 0, ForceSsd1677 = 1, ForceUc8179 = 2 };

DisplayControllerOverride readDisplayControllerOverride() {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) {
    return DisplayControllerOverride::Auto;
  }
  const uint8_t raw = prefs.getUChar(NVS_KEY_DISP_OVERRIDE, static_cast<uint8_t>(DisplayControllerOverride::Auto));
  prefs.end();
  if (raw > static_cast<uint8_t>(DisplayControllerOverride::ForceUc8179)) {
    return DisplayControllerOverride::Auto;
  }
  return static_cast<DisplayControllerOverride>(raw);
}

const char* displayControllerName(BoardConfig::DisplayController c) {
  switch (c) {
    case BoardConfig::DisplayController::SSD1677: return "SSD1677";
    case BoardConfig::DisplayController::UC8253: return "UC8253";
    case BoardConfig::DisplayController::ED2208: return "ED2208";
    case BoardConfig::DisplayController::LgfxEpd: return "LgfxEpd";
    case BoardConfig::DisplayController::IT8951: return "IT8951";
    case BoardConfig::DisplayController::UC8279: return "UC8279";
    case BoardConfig::DisplayController::UC8179: return "UC8179";
  }
  return "unknown";
}

// Resolves BoardConfig::ACTIVE.displayController and logs, on every boot, which
// of three sources decided it — so a misread on silicon the bus probe has never
// run against before (X4 Pro, see the FREEINK_DEVICE_X4PRO block in begin()
// below) is a one-line serial read, not a guess:
//   - "override"          : forced via NVS cphw/disp_ovr, a recovery/support
//                            escape hatch. The probe never runs in this case.
//   - "bus probe"         : freeink::applyXteinkDisplayController() confirmed
//                            an UltraChip (UC81xx) part via the live half-duplex
//                            SPI read and promoted the profile default.
//   - "fallback default"  : probe found no UltraChip signature; the profile's
//                            compile-time default (BoardConfig::ACTIVE literal)
//                            stands unchanged.
// The OEM hw_calib/screenType NVS value is read and logged separately, inside
// applyXteinkDisplayController() itself — it is diagnostic-only by design
// (unreliable in the field: a full-flash from another unit overwrites it), so
// it never decides the outcome and deliberately never appears as a source here.
void applyDisplayControllerWithOverride() {
  const DisplayControllerOverride ovr = readDisplayControllerOverride();
  if (ovr == DisplayControllerOverride::ForceSsd1677 || ovr == DisplayControllerOverride::ForceUc8179) {
    BoardConfig::ACTIVE.displayController = ovr == DisplayControllerOverride::ForceSsd1677
                                                 ? BoardConfig::DisplayController::SSD1677
                                                 : BoardConfig::DisplayController::UC8179;
    BoardConfig::ACTIVE.displayControllerVariant = 0;
    g_displayControllerSource = "override";
    LOG_INF("HW", "Display controller: %s (source: override, probe skipped)",
            displayControllerName(BoardConfig::ACTIVE.displayController));
    return;
  }

  const bool promoted = freeink::applyXteinkDisplayController();
  g_displayControllerSource = promoted ? "bus probe" : "fallback default";
  LOG_INF("HW", "Display controller: %s (source: %s)", displayControllerName(BoardConfig::ACTIVE.displayController),
          g_displayControllerSource);
}

HalGPIO::DeviceType detectDeviceTypeWithFingerprint() {
  // Explicit override for recovery/support:
  // 0 = auto, 1 = force X4, 2 = force X3
  const NvsDeviceValue overrideValue = readNvsDeviceValue(NVS_KEY_DEV_OVERRIDE, NvsDeviceValue::Unknown);
  if (overrideValue == NvsDeviceValue::X3 || overrideValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Device override active: %s", overrideValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(overrideValue);
  }

  const NvsDeviceValue cachedValue = readNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::Unknown);
  if (cachedValue == NvsDeviceValue::X3 || cachedValue == NvsDeviceValue::X4) {
    LOG_INF("HW", "Using cached device type: %s", cachedValue == NvsDeviceValue::X3 ? "X3" : "X4");
    return nvsToDeviceType(cachedValue);
  }

  // No cache yet: use FreeInk's canonical two-pass X3 fingerprint and persist
  // only confirmed results. Inconclusive probes deliberately remain uncached.
  uint8_t score1 = 0;
  uint8_t score2 = 0;
  const freeink::XteinkVerdict verdict = freeink::detectXteinkVerdict(&score1, &score2);
  LOG_INF("HW", "Xteink probe scores: pass1=%u pass2=%u verdict=%u", score1, score2, static_cast<unsigned>(verdict));

  if (verdict == freeink::XteinkVerdict::X3Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X3);
    return HalGPIO::DeviceType::X3;
  }

  if (verdict == freeink::XteinkVerdict::X4Confirmed) {
    writeNvsDeviceValue(NVS_KEY_DEV_CACHED, NvsDeviceValue::X4);
    return HalGPIO::DeviceType::X4;
  }

  // Conservative fallback for first boot with inconclusive probes.
  return HalGPIO::DeviceType::X4;
}

}  // namespace

const char* HalGPIO::getDisplayControllerName() {
  return displayControllerName(BoardConfig::ACTIVE.displayController);
}

const char* HalGPIO::getDisplayControllerSource() { return g_displayControllerSource; }

const char* HalGPIO::setDisplayControllerOverride(uint8_t rawValue) {
  if (rawValue > static_cast<uint8_t>(DisplayControllerOverride::ForceUc8179)) {
    return nullptr;
  }
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) {
    return nullptr;
  }
  prefs.putUChar(NVS_KEY_DISP_OVERRIDE, rawValue);
  prefs.end();
  switch (static_cast<DisplayControllerOverride>(rawValue)) {
    case DisplayControllerOverride::Auto: return "auto";
    case DisplayControllerOverride::ForceSsd1677: return "SSD1677";
    case DisplayControllerOverride::ForceUc8179: return "UC8179";
  }
  return nullptr;
}

void HalGPIO::begin() {
#if FREEINK_MCU_C3
  _deviceType = detectDeviceTypeWithFingerprint();
  BoardConfig::selectDevice(deviceIsX3() ? BoardConfig::Board::XteinkX3 : BoardConfig::Board::XteinkX4);

  // Resolve the per-batch controller before SPI owns the display pins. FreeInk
  // checks the OEM hw_calib/screenType value first, then falls back to its
  // two-pass display-bus probe. X3's facade keys panel selection off the sibling
  // board profile, so preserve a detected UC8279 through setDisplayX3().
  applyDisplayControllerWithOverride();
  if (deviceIsX3() && BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279) {
    BoardConfig::selectDevice(BoardConfig::Board::XteinkX3Uc8279);
  }

  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);

  if (deviceIsX4()) {
    pinMode(BAT_GPIO0, INPUT);
    pinMode(UART0_RXD, INPUT);
  }
#else
  _deviceType = DeviceType::X4;

#if FREEINK_DEVICE_X4PRO
  // X4 Pro carries the same SSD1677/UC8179 batch variance as X4 (C3), but this
  // device is FREEINK_MCU_S3, not FREEINK_MCU_C3, so it fell outside the probe
  // call above entirely — BoardConfig::ACTIVE.displayController stayed pinned
  // at its compile-time SSD1677 default on every boot regardless of the unit's
  // actual silicon. Device-scoped (not FREEINK_MCU_S3-wide): the other S3
  // boards (M5/Murphy/de-link/LilyGo/Sticky) don't have this batch variance.
  // Runs before EpdBus::begin()'s SPI.begin() (FreeInkDisplay.cpp) claims the
  // display pins for the real driver, same ordering constraint as the X3/X4
  // probe above. The probe is stateless (XteinkDetect.cpp writes nothing to
  // NVS), so it re-reads live silicon on every boot — no cached verdict to
  // invalidate. Same override + boot-time outcome log used by the X3/X4 path
  // above; see applyDisplayControllerWithOverride() for the NVS key/log shape.
  applyDisplayControllerWithOverride();
#endif
#endif
  inputMgr.begin();
}

void HalGPIO::update() {
  inputMgr.update();
  const bool connected = isUsbConnected();
  usbStateChanged = (connected != lastUsbConnected);
  lastUsbConnected = connected;
}

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

bool HalGPIO::isPressed(uint8_t buttonIndex) const { return inputMgr.isPressed(buttonIndex); }

bool HalGPIO::wasPressed(uint8_t buttonIndex) const { return inputMgr.wasPressed(buttonIndex); }

bool HalGPIO::wasAnyPressed() const { return inputMgr.wasAnyPressed(); }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const { return inputMgr.wasReleased(buttonIndex); }

bool HalGPIO::wasAnyReleased() const { return inputMgr.wasAnyReleased(); }

unsigned long HalGPIO::getHeldTime() const { return inputMgr.getHeldTime(); }

unsigned long HalGPIO::getPowerButtonHeldTime() const { return inputMgr.getPowerButtonHeldTime(); }

bool HalGPIO::hasTouch() const { return inputMgr.hasTouch(); }

bool HalGPIO::wasTouchTap(float& nx, float& ny) const { return inputMgr.wasTouchTap(nx, ny); }

bool HalGPIO::wasTouchDown(float& nx, float& ny) const { return inputMgr.wasTouchPressedAt(nx, ny); }

bool HalGPIO::isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const {
  return inputMgr.isTouchTapCandidate(nx, ny, heldMs);
}

bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const { return inputMgr.isTouchHeldAt(nx, ny); }

unsigned long HalGPIO::lastTouchHeldMs() const { return inputMgr.lastTouchHeldMs(); }

bool HalGPIO::wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const {
  return inputMgr.wasSwipe(nxStart, nyStart, nxEnd, nyEnd);
}

bool HalGPIO::wasTouchActivity() const { return inputMgr.wasTouchActivity(); }

bool HalGPIO::wasHomeKeyTapped() const { return inputMgr.wasHomeKeyTapped(); }

bool HalGPIO::wasHomeKeyLongPressed() const { return inputMgr.wasHomeKeyLongPressed(); }

void HalGPIO::setSharedConfirmPowerShortPressEmitsPower(const bool enabled) {
  InputManager::setSharedConfirmPowerShortPressEmitsPower(enabled);
}

bool HalGPIO::isXteinkDevice() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4;
}

bool HalGPIO::verifyPowerButtonWakeup() {
  const auto& input = BoardConfig::ACTIVE.input;
  if (input.power < 0) return true;

  const auto powerIsPhysicallyPressed = [&input]() {
    return gpio_policy::isPhysicalButtonPressed(true, input.powerActiveHigh, digitalRead(input.power) == HIGH);
  };

  // Reject a transient wake unless the physical power line remains asserted for
  // the full debounce interval. Raw GPIO is intentional: BTN_POWER is a logical,
  // debounced event and shared confirm/power boards do not expose it directly.
  constexpr unsigned long POWER_WAKE_STABILITY_MS = 10;
  const bool heldAtFirstSample = powerIsPhysicallyPressed();
  const unsigned long sampleStart = millis();
  inputMgr.update();
  while (millis() - sampleStart < POWER_WAKE_STABILITY_MS || inputMgr.isDebouncePending()) {
    delay(1);
    inputMgr.update();
  }
  return gpio_policy::isStablePowerWake(heldAtFirstSample, powerIsPhysicallyPressed());
}

bool HalGPIO::isUsbConnected() const {
  // Charging is a valid USB fallback only on Sticky, whose board profile has a
  // proven active-low BQ25616 CHARGE_STATE pin. X4 Pro's CW2017 reports battery
  // state but cannot report external power, so never infer USB from it.
  const auto source = gpio_policy::selectUsbDetectionSource(deviceIsX3(), BoardConfig::ACTIVE.usbDetect >= 0,
                                                            BoardConfig::isSticky() &&
                                                                BoardConfig::ACTIVE.batteryChargeStatus >= 0);
  switch (source) {
    case gpio_policy::UsbDetectionSource::X3GaugeCurrent:
      // Preserve the X3's BQ27220 Current() semantics and bounded retry.
      for (uint8_t attempt = 0; attempt < 2; ++attempt) {
        int16_t currentMa = 0;
        if (X3GPIO::readBQ27220CurrentMA(&currentMa)) {
          return gpio_policy::usbConnectedFromCurrent(true, currentMa);
        }
        delay(2);
      }
      return false;
    case gpio_policy::UsbDetectionSource::DigitalPin:
      return digitalRead(BoardConfig::ACTIVE.usbDetect) == HIGH;
    case gpio_policy::UsbDetectionSource::ChargingState:
      // Sticky's board-declared BQ25616 CHARGE_STATE is active-low. Read that
      // proven signal directly instead of changing its existing gauge semantics.
      return gpio_policy::usbConnectedFromCharging(
          true, digitalRead(BoardConfig::ACTIVE.batteryChargeStatus) == LOW);
    case gpio_policy::UsbDetectionSource::Unsupported:
    default:
      return false;
  }
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  const auto wakeupCause = esp_sleep_get_wakeup_cause();
  const auto resetReason = esp_reset_reason();

  const bool usbConnected = isUsbConnected();

  if (resetReason == ESP_RST_DEEPSLEEP &&
      (wakeupCause == ESP_SLEEP_WAKEUP_GPIO || wakeupCause == ESP_SLEEP_WAKEUP_EXT1)) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && !usbConnected) {
    return WakeupReason::PowerButton;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_UNKNOWN && usbConnected) {
    return WakeupReason::AfterFlash;
  }
  if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED && resetReason == ESP_RST_POWERON && usbConnected) {
    return WakeupReason::AfterUSBPower;
  }
  return WakeupReason::Other;
}
