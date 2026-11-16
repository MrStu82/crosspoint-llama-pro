#include "ActivityManager.h"
#include "util/ReaderDiagnostics.h"

#include <FontCacheManager.h>
#include <HalDisplay.h>
#include <HalPowerManager.h>
#include <VectorFontSupport.h>

#include <algorithm>
#include <cmath>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "OpdsServerStore.h"
#include "boot_sleep/BootActivity.h"
#include "boot_sleep/SleepActivity.h"
#include "browser/OpdsBookBrowserActivity.h"
#include "components/InkPointShell.h"
#include "game/GamesListActivity.h"
#include "game/MinesweeperActivity.h"
#include "game/SolitaireActivity.h"
#include "game/SudokuActivity.h"
#include "game/TetrisActivity.h"
#include "home/CrashActivity.h"
#include "home/FileBrowserActivity.h"
#include "home/HomeActivity.h"
#include "home/LibraryShelfActivity.h"
#include "home/RecentBooksActivity.h"
#include "home/StatsActivity.h"
#include "network/CrossPointWebServerActivity.h"
#include "reader/ReaderActivity.h"
#include "settings/FrontlightActivity.h"
#include "settings/OpdsServerListActivity.h"
#include "settings/SettingsActivity.h"
#include "util/FullScreenMessageActivity.h"

static portMUX_TYPE activityManagerSpinlock = portMUX_INITIALIZER_UNLOCKED;

void ActivityManager::begin() {
#if defined(configNUM_CORES) && configNUM_CORES > 1
  constexpr BaseType_t renderTaskCore = 1;
#else
  constexpr BaseType_t renderTaskCore = 0;
#endif
#if CROSSPOINT_VECTOR_FONTS
  // FreeType rasterization runs on this task, and the deepest observed chain
  // is a glyph fault DURING LAYOUT: expat + parser + line-layout frames
  // (~3.5KB on Xtensa) with the scan converter's FT_RENDER_POOL_SIZE (4KB)
  // stack-resident band pool on top — a measured ~8KB peak that trips the
  // canary on an 8KB stack. Vector-font boards all have PSRAM-class RAM.
  constexpr uint32_t renderTaskStackBytes = 16384;
#else
  constexpr uint32_t renderTaskStackBytes = 8192;
#endif
  xTaskCreatePinnedToCore(&renderTaskTrampoline, "ActivityManagerRender",
                          renderTaskStackBytes,  // Stack size
                          this,                  // Parameters
                          1,                     // Priority
                          &renderTaskHandle,     // Task handle
                          renderTaskCore  // Keep long renders/cover decodes off CPU 0's idle watchdog when available
  );
  assert(renderTaskHandle != nullptr && "Failed to create render task");
}

void ActivityManager::renderTaskTrampoline(void* param) {
  auto* self = static_cast<ActivityManager*>(param);
  self->renderTaskLoop();
}

void ActivityManager::renderTaskLoop() {
  // Regression gate for the render task's fixed 8192B stack (see
  // FrameDirtyPlanner's cells_/dirty_ member-promotion fix): a periodic
  // high-water-mark log is the on-glass proof the fix bought back headroom,
  // and the assert is the standing trip-wire against the next oversized
  // local reintroducing the same margin problem before it panics for real.
  // `uxTaskGetStackHighWaterMark()` is word-count data; keeping the local
  // storage unsigned also lets the native simulator exercise this trip-wire
  // without depending on an ESP-IDF-only typedef.
  constexpr unsigned int kStackWatermarkLogEveryFrames = 64;
  constexpr unsigned int kStackWatermarkMinWords = 256;  // ~1KB on Xtensa/32-bit words
  static unsigned int renderFrameCount = 0;

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Acquire the lock before reading currentActivity to avoid a TOCTOU race
    // where the main task deletes the activity between the null-check and render().
    RenderLock lock;
    taskENTER_CRITICAL(&activityManagerSpinlock);
    const uint32_t renderGen = ++renderStartGen;
    taskEXIT_CRITICAL(&activityManagerSpinlock);
    if (currentActivity) {
      HalPowerManager::Lock powerLock;  // Ensure we don't go into low-power mode while rendering
      display.setInverted(SETTINGS.screenInverted != 0);
      renderer.setBoldStrokes(SETTINGS.darkerText == 2 && currentActivity->isReaderActivity());
      currentActivity->render(std::move(lock));
    }

    const unsigned int stackWordsFree = uxTaskGetStackHighWaterMark(nullptr);
    renderFrameCount++;
    if (renderFrameCount % kStackWatermarkLogEveryFrames == 0) {
      LOG_DBG("ACT", "render task stack high-water mark: %u words free", static_cast<unsigned>(stackWordsFree));
    }
    assert(stackWordsFree >= kStackWatermarkMinWords && "Render task stack margin critically low");

    // Notify a task blocked in requestUpdateAndWait() once a render that started
    // after it registered is done; an in-flight stale render does not count.
    TaskHandle_t waiter = nullptr;
    taskENTER_CRITICAL(&activityManagerSpinlock);
    if (renderGen > waitRegisteredGen) {
      waiter = waitingTaskHandle;
      waitingTaskHandle = nullptr;
    }
    taskEXIT_CRITICAL(&activityManagerSpinlock);
    if (waiter) {
      xTaskNotify(waiter, 1, eIncrement);
    }
  }
}

#if FREEINK_DEVICE_PICCO
namespace {
// Picco system edge swipes get a cue matching the stock Boox side_edge_* asset
// geometry (144x48 / 48x144, light-grey dome flush on the edge, outlined glyph),
// drawn by our own code and centred on its edge. kDomeRows rows run tip -> edge.
constexpr int kDomeRows = 48;

// Dome half-width at row r (0 = tip, 47 = on the edge): a sqrt bell with a
// radius-22 fillet flaring into the edge (tip ~9px wide, base 135px; within
// 2px of the stock outline on every row).
int piccoDomeHalf(int r) {
  float half = 4.0f + 6.0f * std::sqrt(static_cast<float>(r));
  if (r > 25) {
    const float t = static_cast<float>(r - 25);
    half += 22.0f - std::sqrt(484.0f - t * t);
  }
  return static_cast<int>(half + 0.5f);
}

// Light grey: one black pixel per 8 (every other row, every 4th column, staggered).
bool piccoDomeDither(int u, int v) { return (v & 1) && ((u + 2 * ((v >> 1) & 1)) & 3) == 2; }

// u runs along the edge, d is the distance from the edge (0 = on it).
void piccoDomePixel(const GfxRenderer& r, MappedInputManager::EdgeSwipe edge, bool left, int u, int d, bool black) {
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  if (edge == MappedInputManager::EdgeSwipe::Home) {
    r.drawPixel(u, h - 1 - d, black);
  } else {
    r.drawPixel(left ? d : w - 1 - d, u, black);
  }
}

// c = cue centre along its edge (x for Home, y for Back).
void drawPiccoEdgeCue(const GfxRenderer& r, MappedInputManager::EdgeSwipe edge, bool left, int c) {
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const bool home = edge == MappedInputManager::EdgeSwipe::Home;
  for (int row = 0; row < kDomeRows; ++row) {
    const int d = kDomeRows - 1 - row;
    const int half = piccoDomeHalf(row);
    for (int u = c - half; u <= c + half; ++u) {
      piccoDomePixel(r, edge, left, u, d, piccoDomeDither(u, row));
    }
  }
  if (home) {
    // Outlined house, 2px strokes: apex 35px off the edge, floor 14px off it.
    const int top = h - 35;
    r.drawLine(c - 11, top + 9, c, top, 2, true);
    r.drawLine(c, top, c + 11, top + 9, 2, true);
    r.drawLine(c - 8, top + 9, c - 8, top + 20, 2, true);
    r.drawLine(c + 8, top + 9, c + 8, top + 20, 2, true);
    r.drawLine(c - 8, top + 20, c + 8, top + 20, 2, true);
    r.drawLine(c, top + 13, c, top + 16, 2, true);
  } else {
    // Back chevron pointing off the edge: apex 18px in, arms 9px deeper, 20px tall.
    const int tip = left ? 18 : w - 1 - 18;
    const int arm = left ? tip + 9 : tip - 9;
    r.drawLine(arm, c - 10, tip, c, 2, true);
    r.drawLine(tip, c, arm, c + 10, 2, true);
  }
}
}  // namespace

void ActivityManager::trackPiccoEdgeCue() {
  // Moves closer than this are dropped, as are moves while a render holds the panel.
  constexpr int kCueMoveMinPx = 8;
  int fx = 0;
  int fy = 0;
  int sx = 0;
  const auto edge = mappedInput.piccoEdgeHeld(fx, fy, sx);
  if (edge == MappedInputManager::EdgeSwipe::None || edgeCueFailed || RenderLock::peek()) return;
  RenderLock lock;
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const bool home = edge == MappedInputManager::EdgeSwipe::Home;
  const bool left = sx < w / 2;
  const int half = piccoDomeHalf(kDomeRows - 1);
  const int c = std::clamp(home ? fx : fy, half, (home ? w : h) - 1 - half);
  if (edgeCueTracked && std::abs(c - edgeCueAlong) < kCueMoveMinPx) return;
  int* b = edgeCueRect;
  if (!edgeCueTracked) {
    b[0] = home ? 0 : (left ? 0 : w - kDomeRows);
    b[1] = home ? h - kDomeRows : 0;
    b[2] = home ? w : kDomeRows;
    b[3] = home ? kDomeRows : h;
    // Long side x (short side + 8-px alignment slack) bits.
    edgeCueBand.resize(static_cast<size_t>(std::max(w, h)) * (kDomeRows + 8) / 8);
    if (renderer.readFramebufferRegion(b[0], b[1], b[2], b[3], edgeCueBand.data(), edgeCueBand.size()) == 0) {
      edgeCueFailed = true;
      return;
    }
    edgeCueTracked = true;
    edgeCueActivity = currentActivity.get();
  } else {
    renderer.writeFramebufferRegion(b[0], b[1], b[2], b[3], edgeCueBand.data());
  }
  drawPiccoEdgeCue(renderer, edge, left, c);
  // Same full-frame fast refresh as the one-shot cue; synchronous, so moves made meanwhile are dropped.
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  edgeCueAlong = c;
}
#endif

void ActivityManager::loop() {
  // The brightness sheet is a global overlay, not an Activity — while it's open it
  // must get first refusal on every input frame (mirroring how an Activity's own
  // OptionPopup is checked before its normal loop() body), and no other gesture or
  // per-activity handling below may run in the same frame.
  if (brightnessSheet.isOpen()) {
    const bool consumed = brightnessSheet.loop();
    if (!brightnessSheet.isOpen() && currentActivity) currentActivity->onUncovered();
    if (consumed) return;
  }

  if (currentActivity) {
#if FREEINK_DEVICE_PICCO
    trackPiccoEdgeCue();
    int edgeX = 0;
    const auto edge = mappedInput.piccoEdgeSwipe(&edgeX);
    if (edge != MappedInputManager::EdgeSwipe::None) {
      if (!edgeCueTracked) {  // flick, or the band save failed: one-shot centred cue
        RenderLock lock;
        const bool home = edge == MappedInputManager::EdgeSwipe::Home;
        drawPiccoEdgeCue(renderer, edge, edgeX < renderer.getScreenWidth() / 2,
                         (home ? renderer.getScreenWidth() : renderer.getScreenHeight()) / 2);
        renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      }
      edgeCueShown = true;
    }
#endif
    if (!currentActivity->isHomeActivity() && mappedInput.wasHomeGesture()) {
      if (currentActivity->handleHomeGesture()) {
        return;
      }
      goHome();
      return;
    }

    // Global left-edge upward swipe -> backlight quick-settings drawer. Checked here, once, for
    // every activity, rather than duplicated per-activity. Fires before dispatch so it wins over
    // any local touch handling; suppressed while FrontlightActivity is itself on top so a swipe
    // inside the brightness panel can't reopen it.
    if (!currentActivity->isFrontlightActivity() && mappedInput.wasBrightnessGesture()) {
      currentActivity->startActivityForResult(std::make_unique<FrontlightActivity>(renderer, mappedInput),
                                              [this](const ActivityResult&) { requestUpdate(); });
      return;
    }

    // Quick settings has one unambiguous entry path: a downward swipe whose
    // first sample is in the top-edge zone. Bottom-edge up is reserved for the
    // reader's Text Settings drawer, and ordinary status-bar taps stay inert.
    if (!currentActivity->isFrontlightActivity() && mappedInput.wasMenuGesture()) {
      currentActivity->onCovered();
      brightnessSheet.open();
      return;
    }

    // Note: do not hold a lock here, the loop() method must be responsible for acquire one if needed
    currentActivity->loop();
  }

  while (pendingAction != PendingAction::None) {
    if (pendingAction == PendingAction::Pop) {
      RenderLock lock;

      if (!currentActivity) {
        // Should never happen in practice
        LOG_ERR("ACT", "Pop set but currentActivity is null; ignoring pop request");
        pendingAction = PendingAction::None;
        continue;
      }

      ActivityResult pendingResult = std::move(currentActivity->result);

      // Destroy the current activity
      exitActivity(lock);
      pendingAction = PendingAction::None;

      if (stackActivities.empty()) {
        LOG_DBG("ACT", "No more activities on stack, going home");
        lock.unlock();  // goHome may acquire its own lock
        goHome();
        continue;  // Will launch goHome immediately

      } else {
        currentActivity = std::move(stackActivities.back());
        stackActivities.pop_back();
        LOG_DBG("ACT", "Popped from activity stack, new size = %zu", stackActivities.size());
        // Handle result if necessary
        if (currentActivity->resultHandler) {
          LOG_DBG("ACT", "Handling result for popped activity");

          // Move it here to avoid the case where handler calling another startActivityForResult()
          auto handler = std::move(currentActivity->resultHandler);
          currentActivity->resultHandler = nullptr;
          lock.unlock();  // Handler may acquire its own lock
          handler(pendingResult);
        }

        if (pendingAction == PendingAction::None) currentActivity->onUncovered();

        // Request an update to ensure the popped activity gets re-rendered
        if (pendingAction == PendingAction::None) {
          requestUpdate();
        }

        // Handler may request another pending action, we will handle it in the next loop iteration
        continue;
      }

    } else if (pendingActivity) {
      // Current activity has requested a new activity to be launched
      RenderLock lock;

      if (pendingAction == PendingAction::Replace) {
        // Destroy the current activity
        exitActivity(lock);
        // Clear the stack
        while (!stackActivities.empty()) {
          stackActivities.back()->onExit();
          stackActivities.pop_back();
        }
      } else if (pendingAction == PendingAction::Push) {
        // Move current activity to stack
        if (currentActivity) currentActivity->onCovered();
        stackActivities.push_back(std::move(currentActivity));
        LOG_DBG("ACT", "Pushed to activity stack, new size = %zu", stackActivities.size());
      }
      pendingAction = PendingAction::None;
      currentActivity = std::move(pendingActivity);

      lock.unlock();  // onEnter may acquire its own lock
      currentActivity->onEnter();

      // onEnter may request another pending action, we will handle it in the next loop iteration
      continue;
    }
  }

#if FREEINK_DEVICE_PICCO
  // Repaint whatever is current once the edge gesture's action has run, so the
  // cue never stays on glass (e.g. a Back the screen ignores).
  if (edgeCueShown && pendingAction == PendingAction::None) {
    edgeCueShown = false;
    // Same screen and nothing else to draw: put the saved band back and fast-refresh.
    if (edgeCueTracked && currentActivity.get() == edgeCueActivity && !requestedUpdate) {
      RenderLock lock;
      const int* b = edgeCueRect;
      renderer.writeFramebufferRegion(b[0], b[1], b[2], b[3], edgeCueBand.data());
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    } else {
      requestedUpdate = true;
    }
    edgeCueTracked = false;
    edgeCueFailed = false;
  }
#endif
  if (requestedUpdate.exchange(false)) {
    // Using direct notification to signal the render task to update
    // Increment counter so multiple rapid calls won't be lost
    if (renderTaskHandle) {
      xTaskNotify(renderTaskHandle, 1, eIncrement);
    }
  }
}

void ActivityManager::exitActivity(const RenderLock& lock) {
  // Note: lock must be held by the caller
  if (currentActivity) {
    currentActivity->onExit();
    currentActivity.reset();
  }
}

void ActivityManager::replaceActivity(std::unique_ptr<Activity>&& newActivity) {
  // Note: no lock here, this is usually called by loop() and we may run into deadlock
  if (currentActivity) {
    // Defer launch if we're currently in an activity, to avoid deleting the current activity
    // leading to the "delete this" problem
    pendingActivity = std::move(newActivity);
    pendingAction = PendingAction::Replace;
  } else {
    // No current activity, safe to launch immediately
    currentActivity = std::move(newActivity);
    currentActivity->onEnter();
  }
}

void ActivityManager::goToFileTransfer() {
  replaceActivity(std::make_unique<CrossPointWebServerActivity>(renderer, mappedInput));
}

void ActivityManager::goToSettings() { replaceActivity(std::make_unique<SettingsActivity>(renderer, mappedInput)); }

void ActivityManager::goToStats() { replaceActivity(std::make_unique<StatsActivity>(renderer, mappedInput)); }

void ActivityManager::goToFileBrowser(std::string path) {
  replaceActivity(std::make_unique<FileBrowserActivity>(renderer, mappedInput, std::move(path)));
}

void ActivityManager::goToRecentBooks() {
  // Shelf is the touch (InkPoint) layout's opt-in view; List stays the default.
  if (APP_STATE.libraryShelfView && InkPointShell::enabled(renderer)) {
    replaceActivity(std::make_unique<LibraryShelfActivity>(renderer, mappedInput));
    return;
  }
  replaceActivity(std::make_unique<RecentBooksActivity>(renderer, mappedInput));
}

void ActivityManager::goToBrowser() {
  const auto& servers = OPDS_STORE.getServers();
  // Skip the server picker when there's only one server configured
  if (servers.size() == 1) {
    replaceActivity(std::make_unique<OpdsBookBrowserActivity>(renderer, mappedInput, servers[0]));
  } else {
    replaceActivity(std::make_unique<OpdsServerListActivity>(renderer, mappedInput, true));
  }
}

void ActivityManager::goToReader(std::string path, const bool allowFastInitialRefresh) {
  replaceActivity(std::make_unique<ReaderActivity>(renderer, mappedInput, std::move(path), allowFastInitialRefresh));
}

void ActivityManager::goToSleep(bool fromTimeout) {
  // An open brightness sheet consumes every loop() and would block the Replace
  // below; repaint the activity underneath so a quick-resume frame excludes it.
  if (brightnessSheet.isOpen()) {
    brightnessSheet.close();
    if (currentActivity) currentActivity->onUncovered();
    requestUpdateAndWait();
  }
  replaceActivity(std::make_unique<SleepActivity>(renderer, mappedInput, fromTimeout));
  loop();  // Important: sleep screen must be rendered immediately, the caller will go to sleep right after this returns
}

void ActivityManager::goToBoot() { replaceActivity(std::make_unique<BootActivity>(renderer, mappedInput)); }

void ActivityManager::goToFullScreenMessage(std::string message, EpdFontFamily::Style style) {
  replaceActivity(std::make_unique<FullScreenMessageActivity>(renderer, mappedInput, std::move(message), style));
}

void ActivityManager::goHome(HomeMenuItem initialMenuItem, bool cleanInitialRefresh) {
  if (initialMenuItem == HomeMenuItem::NONE && currentActivity) {
    const auto& activityName = currentActivity->name;
    if (activityName == "FileBrowser") {
      initialMenuItem = HomeMenuItem::FILE_BROWSER;
    } else if (activityName == "RecentBooks" || activityName == "LibraryShelf") {
      initialMenuItem = HomeMenuItem::RECENTS;
    } else if (activityName == "OpdsBookBrowser") {
      initialMenuItem = HomeMenuItem::OPDS_BROWSER;
    } else if (activityName == "CrossPointWebServer") {
      initialMenuItem = HomeMenuItem::FILE_TRANSFER;
    } else if (activityName == "Settings") {
      initialMenuItem = HomeMenuItem::SETTINGS_MENU;
    }
  }
  replaceActivity(std::make_unique<HomeActivity>(renderer, mappedInput, initialMenuItem, cleanInitialRefresh));
}
void ActivityManager::goToCrashReport() { replaceActivity(std::make_unique<CrashActivity>(renderer, mappedInput)); }

void ActivityManager::goToGames() {
  replaceActivity(std::make_unique<GamesListActivity>(renderer, mappedInput));
}

void ActivityManager::goToTetris() {
  replaceActivity(std::make_unique<TetrisActivity>(renderer, mappedInput));
}

void ActivityManager::goToSolitaire() {
  replaceActivity(std::make_unique<SolitaireActivity>(renderer, mappedInput));
}

void ActivityManager::goToMinesweeper() {
  replaceActivity(std::make_unique<MinesweeperActivity>(renderer, mappedInput));
}

void ActivityManager::goToSudoku() {
  replaceActivity(std::make_unique<SudokuActivity>(renderer, mappedInput));
}

void ActivityManager::pushActivity(std::unique_ptr<Activity>&& activity) {
  if (pendingActivity) {
    // Should never happen in practice
    LOG_ERR("ACT", "pendingActivity while pushActivity is not expected");
    pendingActivity.reset();
  }
  pendingActivity = std::move(activity);
  pendingAction = PendingAction::Push;
}

void ActivityManager::popActivity() {
  if (pendingActivity) {
    // Should never happen in practice
    LOG_ERR("ACT", "pendingActivity while popActivity is not expected");
    pendingActivity.reset();
  }
  pendingAction = PendingAction::Pop;
}

bool ActivityManager::preventAutoSleep() const { return currentActivity && currentActivity->preventAutoSleep(); }

bool ActivityManager::isReaderActivity() const {
  return std::any_of(stackActivities.begin(), stackActivities.end(),
                     [](const auto& activity) { return activity->isReaderActivity(); }) ||
         (currentActivity && currentActivity->isReaderActivity());
}

bool ActivityManager::handleForcedRefresh() { return currentActivity && currentActivity->handleForcedRefresh(); }

bool ActivityManager::skipLoopDelay() const { return currentActivity && currentActivity->skipLoopDelay(); }

ScreenshotInfo ActivityManager::getScreenshotInfo() const {
  if (currentActivity) {
    return currentActivity->getScreenshotInfo();
  }
  return {};
}

void ActivityManager::requestUpdate(bool immediate) {
  if (immediate) {
    if (renderTaskHandle) {
      xTaskNotify(renderTaskHandle, 1, eIncrement);
    }
  } else {
    // Deferring the update until current loop is finished
    // This is to avoid multiple updates being requested in the same loop
    requestedUpdate = true;
  }
}
void ActivityManager::requestUpdateAndWait() {
  if (!renderTaskHandle) {
    return;
  }

  // Atomic section to perform checks
  taskENTER_CRITICAL(&activityManagerSpinlock);
  auto currTaskHandler = xTaskGetCurrentTaskHandle();
  auto mutexHolder = xSemaphoreGetMutexHolder(renderingMutex);
  bool isRenderTask = (currTaskHandler == renderTaskHandle);
  bool alreadyWaiting = (waitingTaskHandle != nullptr);
  bool holdingRenderLock = (mutexHolder == currTaskHandler);
  if (!alreadyWaiting && !isRenderTask && !holdingRenderLock) {
    waitingTaskHandle = currTaskHandler;
    waitRegisteredGen = renderStartGen;
  }
  taskEXIT_CRITICAL(&activityManagerSpinlock);

  // Render task cannot call requestUpdateAndWait() or it will cause a deadlock
  assert(!isRenderTask && "Render task cannot call requestUpdateAndWait()");

  // There should never be the case where 2 tasks are waiting for a render at the same time
  assert(!alreadyWaiting && "Already waiting for a render to complete");

  // Cannot call while holding RenderLock or it will cause a deadlock
  assert(!holdingRenderLock && "Cannot call requestUpdateAndWait() while holding RenderLock");

  xTaskNotify(renderTaskHandle, 1, eIncrement);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

// RenderLock

RenderLock::RenderLock() {
  reader_diagnostics::Scope profile(reader_diagnostics::Stage::RenderLock);
  xSemaphoreTake(activityManager.renderingMutex, portMAX_DELAY);
  isLocked = true;
}

RenderLock::RenderLock([[maybe_unused]] Activity&) {
  reader_diagnostics::Scope profile(reader_diagnostics::Stage::RenderLock);
  xSemaphoreTake(activityManager.renderingMutex, portMAX_DELAY);
  isLocked = true;
}

RenderLock::~RenderLock() {
  if (isLocked) {
    xSemaphoreGive(activityManager.renderingMutex);
    isLocked = false;
  }
}

void RenderLock::unlock() {
  if (isLocked) {
    xSemaphoreGive(activityManager.renderingMutex);
    isLocked = false;
  }
}

/**
 *
 * Checks if renderingMutex is busy.
 *
 * @return true if renderingMutex is busy, otherwise false.
 *
 */
bool RenderLock::peek() { return xQueuePeek(activityManager.renderingMutex, NULL, 0) != pdTRUE; };
