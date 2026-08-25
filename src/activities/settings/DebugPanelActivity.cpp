#include "DebugPanelActivity.h"

#include <Arduino.h>
#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

#ifndef FIRMWARE_GIT_SHA
#define FIRMWARE_GIT_SHA "unknown"
#endif

void DebugPanelActivity::onEnter() { Activity::onEnter(); }

void DebugPanelActivity::onExit() { Activity::onExit(); }

void DebugPanelActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
  }
}

void DebugPanelActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_DEBUG_PANEL));

  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int leftX = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  char buf[96];
  char line[96];

  auto row = [&](const char* label, const char* value) {
    renderer.drawText(UI_10_FONT_ID, leftX, y, label, true, EpdFontFamily::BOLD);
    snprintf(line, sizeof(line), ": %s", value);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, leftX + labelWidth, y, line, true);
    y += lineHeight;
  };

  #ifdef SIMULATOR
  snprintf(buf, sizeof(buf), "simulator (X4 Pro)");
  #else
  snprintf(buf, sizeof(buf), "%s (%s)", HalGPIO::getDisplayControllerName(), HalGPIO::getDisplayControllerSource());
  #endif
  row(tr(STR_DEBUG_DISPLAY), buf);

  snprintf(buf, sizeof(buf), "%dx%d, 1-bit", renderer.getScreenWidth(), renderer.getScreenHeight());
  row(tr(STR_DEBUG_RESOLUTION), buf);

  #ifdef SIMULATOR
  snprintf(buf, sizeof(buf), "native host");
  #else
  snprintf(buf, sizeof(buf), "%s rev %d", ESP.getChipModel(), ESP.getChipRevision());
  #endif
  row(tr(STR_DEBUG_CHIP), buf);

  #ifdef SIMULATOR
  snprintf(buf, sizeof(buf), "host-backed");
  #else
  snprintf(buf, sizeof(buf), "%u MB", static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)));
  #endif
  row(tr(STR_DEBUG_FLASH), buf);

  #ifdef SIMULATOR
  snprintf(buf, sizeof(buf), "host-backed");
  #else
  snprintf(buf, sizeof(buf), "%u MB", static_cast<unsigned>(ESP.getPsramSize() / (1024 * 1024)));
  #endif
  row(tr(STR_DEBUG_PSRAM), buf);

  row(tr(STR_DEBUG_FIRMWARE), FIRMWARE_GIT_SHA);

  row(tr(STR_DEBUG_BUILD_DATE), __DATE__ " " __TIME__);

  snprintf(buf, sizeof(buf), "%u KB", static_cast<unsigned>(ESP.getFreeHeap() / 1024));
  row(tr(STR_DEBUG_FREE_HEAP), buf);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
