#pragma once

class WifiAwakeLock {
 public:
  using GetSleep = bool (*)();
  using SetSleep = void (*)(bool);

  WifiAwakeLock();
  WifiAwakeLock(GetSleep getSleep, SetSleep setSleep);
  ~WifiAwakeLock();

  WifiAwakeLock(const WifiAwakeLock&) = delete;
  WifiAwakeLock& operator=(const WifiAwakeLock&) = delete;
  WifiAwakeLock(WifiAwakeLock&&) = delete;
  WifiAwakeLock& operator=(WifiAwakeLock&&) = delete;

  void acquire();
  void release();
  bool isHeld() const { return held; }

 private:
  GetSleep getSleep;
  SetSleep setSleep;
  bool restoreSleep = true;
  bool held = false;
};
