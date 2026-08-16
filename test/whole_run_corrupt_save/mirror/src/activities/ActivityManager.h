#pragma once
// Shadow of src/activities/ActivityManager.h, reused verbatim from
// test/sponsor_hp_clamp/mirror/src/activities/ActivityManager.h. The real header
// cannot compile on host at all (pulls freertos/FreeRTOS.h et al). The tested
// path (onEnter() -> loop() -> resolveWholeRunCorruptNotice()) never actually
// calls into activityManager, but Activity.cpp/GameActivity.cpp are single
// translation units, so every activityManager.* call site anywhere in those
// files must still resolve at compile+link time even though none of them
// execute for this harness's scripted input. All bodies here are inert no-ops.
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
