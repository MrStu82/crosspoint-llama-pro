#pragma once
// Shadow of src/activities/ActivityManager.h. The real header cannot compile on host at all
// (pulls freertos/FreeRTOS.h et al). GameTitleActivity::render() -- the harness's actual
// runtime path -- never touches activityManager; only onEnter()/loop() do, and Activity.cpp's
// only runtime-reachable function from the harness (~Activity() -> exitGameLutMode()) never
// calls into it either. But Activity.cpp is one translation unit, so every activityManager.*
// call it makes anywhere in the file must still resolve at compile+link time even though
// none of them execute. All bodies here are inert no-ops.
#include <memory>
#include <string>

class Activity;
class RenderLock;
class GfxRenderer;
class MappedInputManager;

enum class HomeMenuItem { NONE, FILE_BROWSER, RECENTS, OPDS_BROWSER, FILE_TRANSFER, SETTINGS_MENU, STATS, GAMES };

class ActivityManager {
 public:
  ActivityManager() = default;

  void requestUpdate(bool immediate = false) { (void)immediate; }
  void requestUpdateAndWait() {}
  void goHome(HomeMenuItem item = HomeMenuItem::NONE) { (void)item; }
  void goToReader(const std::string& path) { (void)path; }
  void pushActivity(std::unique_ptr<Activity>&& activity) { (void)activity; }
  void popActivity() {}
  void replaceActivity(std::unique_ptr<Activity>&& newActivity) { (void)newActivity; }
};

inline ActivityManager activityManager;
