#include "UsbTransferActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "activities/home/StatsActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void UsbTransferActivity::onEnter() {
  Activity::onEnter();
  wasConnected = false;

  auto* dev = Storage.rawBlockDeviceForUsbMsc();
  if (!dev || !usbMsc.begin(dev)) {
    LOG_ERR("USB", "UsbTransferActivity: begin() failed (dev=%p)", (void*)dev);
    state = State::ERROR;
    return;
  }

  LOG_INF("USB", "UsbTransferActivity: MSC active, waiting for host");
  state = State::WAITING;
}

void UsbTransferActivity::onExit() {
  // Defensive: guarantee MSC is torn down and the app FS is remounted even if
  // we get exited some way other than endSessionAndFinish() (e.g. a future
  // global "go home" shortcut). Idempotent if already ended.
  if (usbMsc.active()) {
    usbMsc.end();
    Storage.begin();
  }
  // A completed MSC session is the only real event that can change what's on
  // the SD card, so this is the single point where StatsActivity's cached
  // book count needs invalidating. No-op if the session never actually
  // started (state == ERROR).
  if (state != State::ERROR) {
    StatsActivity::invalidateBookCountCache();
  }
  Activity::onExit();
}

void UsbTransferActivity::endSessionAndFinish() {
  LOG_INF("USB", "UsbTransferActivity: ending session");
  usbMsc.end();
  Storage.begin();
  finish();
}

void UsbTransferActivity::loop() {
  if (state == State::ERROR) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      finish();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    endSessionAndFinish();
    return;
  }

  const bool connected = usbMsc.hostConnected();
  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) {
      state = State::CONNECTED;
      requestUpdate();
    } else {
      // Was connected, host has now unmounted/unplugged — end cleanly.
      endSessionAndFinish();
    }
  }
}

void UsbTransferActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_USB_TRANSFER));

  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - lineHeight) / 2;

  if (state == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_USB_TRANSFER_ERROR), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == State::CONNECTED) {
    const Rect bounds{metrics.contentSidePadding, top, pageWidth - metrics.contentSidePadding * 2,
                      pageHeight - top};
    UITheme::drawCenteredWrappedText(renderer, bounds, UI_10_FONT_ID, tr(STR_USB_TRANSFER_CONNECTED), 3, true,
                                     EpdFontFamily::BOLD, UITheme::TextVerticalAlignment::TOP);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_USB_TRANSFER_WAITING));
  }

  if (state != State::ERROR) {
    const int hintY = pageHeight - lineHeight - metrics.verticalSpacing;
    renderer.drawCenteredText(UI_10_FONT_ID, hintY, tr(STR_USB_TRANSFER_HINT));
  }

  renderer.displayBuffer();
}
