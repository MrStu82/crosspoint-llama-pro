#pragma once
#include <Logging.h>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

#include "ActivityManager.h"  // for using the ActivityManager singleton
#include "ActivityResult.h"
#include "GfxRenderer.h"
#include "MappedInputManager.h"
#include "RenderLock.h"
#include "util/ScreenshotInfo.h"

class Activity {
  friend class ActivityManager;

 protected:
  std::string name;
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;

  ActivityResultHandler resultHandler;
  ActivityResult result;

  // Game-mode custom LUT lifecycle. Modeled on GfxRenderer's storeBwBuffer()/restoreBwBuffer()
  // store/restore discipline. enterGameLutMode() is called by a game activity's onEnter();
  // exitGameLutMode() is idempotent and is called both from Activity::onExit() (timely restore
  // on a normal exit) and from ~Activity() (structural backstop so a crash mid-frame or a
  // subclass onExit() override that forgets to chain up can never strand a custom LUT in the
  // shared controller for the next activity to inherit).
  bool gameLutActive = false;
  void enterGameLutMode(const unsigned char* lutData = nullptr);
  void exitGameLutMode();

 public:
  explicit Activity(std::string name, GfxRenderer& renderer, MappedInputManager& mappedInput)
      : name(std::move(name)), renderer(renderer), mappedInput(mappedInput) {}
  // The destructor (not onEnter/onExit override chaining) is the backstop that guarantees
  // exitGameLutMode() runs: ~Activity() always executes after any derived destructor,
  // regardless of whether a subclass's onExit() override forgets to chain to Activity::onExit().
  virtual ~Activity() { exitGameLutMode(); }
  virtual void onEnter();
  virtual void onExit();
  virtual void loop() {}

  virtual void render(RenderLock&&) {}

  // If immediate is true, the update will be triggered immediately.
  // Otherwise, it will be deferred until the end of the current loop iteration.
  virtual void requestUpdate(bool immediate = false);

  // Request an immediate render and block until it completes.
  virtual void requestUpdateAndWait();

  virtual bool skipLoopDelay() { return false; }
  virtual bool preventAutoSleep() { return false; }
  virtual bool isReaderActivity() const { return false; }
  // Returns true when the activity schedules its own forced refresh.
  virtual bool handleForcedRefresh() { return false; }
  virtual bool isHomeActivity() const { return false; }
  virtual bool handleHomeGesture() { return false; }
  // True for FrontlightActivity itself, so the global brightness gesture (dispatched by
  // ActivityManager) doesn't reopen it while it's already the panel on top.
  virtual bool isFrontlightActivity() const { return false; }
  virtual ScreenshotInfo getScreenshotInfo() const { return {}; }

  // Start a new activity without destroying the current one
  // Note: requestUpdate() will be invoked automatically once resultHandler finishes
  void startActivityForResult(std::unique_ptr<Activity>&& activity, ActivityResultHandler resultHandler);

  // Set the result to be passed back to the previous activity when this activity finishes
  void setResult(ActivityResult&& result);

  // Finish this activity and return to the previous one on the stack (if any)
  void finish();

  // Convenience method to facilitate API transition to ActivityManager
  // TODO: remove this in near future
  void onGoHome(HomeMenuItem item = HomeMenuItem::NONE);
  void onSelectBook(const std::string& path);

 protected:
  enum class ListTouchResult : uint8_t {
    None,      // touch did not hit the list
    Consumed,  // touchdown moved the highlight (repaint already requested)
    Activated  // tap landed on a row: selectedIndex is updated, caller activates it
  };

  // Shared touch handling for selectable list screens: touchdown highlights the
  // touched row, a tap selects and reports Activated. The caller supplies the
  // list band and runs its own activate action on Activated.
  ListTouchResult handleListTouch(int& selectedIndex, int itemCount, int listTop, int listHeight, bool hasSubtitle);
};
