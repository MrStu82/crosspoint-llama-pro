#pragma once

// Minimal stand-in for the real lib/hal/HalStorage.h, which pulls in Arduino
// Print/String and FreeRTOS headers. AchievementBus.cpp only ever touches
// mkdir/openFileForRead/openFileForWrite through the `Storage` macro -- the
// test has no filesystem, so every call just reports failure and load()/
// save() take their existing "no file" / "couldn't open" fallback paths,
// which is exactly what a fresh install looks like anyway.

class HalFile {
 public:
  bool isOpen() const { return false; }
  operator bool() const { return false; }
};

class HalStorage {
 public:
  inline static int writeOpenCalls = 0;

  bool mkdir(const char*) { return true; }
  bool openFileForRead(const char*, const char*, HalFile&) { return false; }
  bool openFileForWrite(const char*, const char*, HalFile&) {
    writeOpenCalls++;
    return false;
  }

  static void resetWriteOpenCalls() { writeOpenCalls = 0; }

  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
};

#define Storage HalStorage::getInstance()
