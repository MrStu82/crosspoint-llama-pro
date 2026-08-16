#pragma once
// Test double for the real lib/hal/HalStorage.h, extending the pattern already
// proven in /workspace/agent/ach_test/mocks/HalStorage.h (real filesystem I/O,
// not simulated -- a round trip here is a genuine write-close-reopen-read).
// Extended beyond that version with close()/exists()/remove(), which
// GameState.cpp/GameSave.cpp call and AchievementBus.cpp's narrower surface
// didn't need.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

class HalFile {
 public:
  FILE* fp = nullptr;

  HalFile() = default;
  ~HalFile() {
    if (fp) fclose(fp);
  }
  HalFile(HalFile&& other) noexcept : fp(other.fp) { other.fp = nullptr; }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this != &other) {
      if (fp) fclose(fp);
      fp = other.fp;
      other.fp = nullptr;
    }
    return *this;
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  size_t write(const void* buf, size_t count) {
    if (!fp) return 0;
    return fwrite(buf, 1, count, fp);
  }
  int read(void* buf, size_t count) {
    if (!fp) return -1;
    return static_cast<int>(fread(buf, 1, count, fp));
  }
  int read() {
    if (!fp) return -1;
    int c = fgetc(fp);
    return c;
  }
  bool seek(size_t pos) {
    if (!fp) return false;
    return fseek(fp, static_cast<long>(pos), SEEK_SET) == 0;
  }
  bool seekSet(size_t pos) { return seek(pos); }
  bool seekCur(int64_t offset) {
    if (!fp) return false;
    return fseek(fp, static_cast<long>(offset), SEEK_CUR) == 0;
  }
  bool close() {
    if (!fp) return false;
    bool ok = fclose(fp) == 0;
    fp = nullptr;
    return ok;
  }
  operator bool() const { return fp != nullptr; }
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool mkdir(const char* path, const bool pFlag = true) {
    (void)pFlag;
    std::string cur = root;
    ::mkdir(cur.c_str(), 0755);
    std::string p(path);
    size_t pos = 1;
    while ((pos = p.find('/', pos)) != std::string::npos) {
      cur = root + p.substr(0, pos);
      ::mkdir(cur.c_str(), 0755);
      pos++;
    }
    std::string full = root + path;
    ::mkdir(full.c_str(), 0755);
    return true;
  }

  bool openFileForRead(const char* moduleName, const char* path, HalFile& file) {
    (void)moduleName;
    std::string full = root + path;
    FILE* f = fopen(full.c_str(), "rb");
    if (!f) return false;
    file.fp = f;
    return true;
  }

  bool openFileForWrite(const char* moduleName, const char* path, HalFile& file) {
    (void)moduleName;
    std::string full = root + path;
    FILE* f = fopen(full.c_str(), "wb");
    if (!f) return false;
    file.fp = f;
    return true;
  }

  bool exists(const char* path) {
    std::string full = root + path;
    struct stat st;
    return ::stat(full.c_str(), &st) == 0;
  }

  bool remove(const char* path) {
    std::string full = root + path;
    return ::remove(full.c_str()) == 0;
  }

  // Test-only knob: point the mock SD root at a real temp dir. Set before any
  // call above; not thread-safe (single-threaded harness only).
  std::string root = "/tmp/ach_test_sd";
};

#define Storage HalStorage::getInstance()
