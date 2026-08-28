#pragma once

#include <cstdint>
#include <cstring>
#include <string>

using oflag_t = uint32_t;
constexpr oflag_t O_WRITE = 1u << 0;
constexpr oflag_t O_CREAT = 1u << 1;
constexpr oflag_t O_TRUNC = 1u << 2;

namespace StyledStorageStub {
inline std::string staged;
}

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(bool value) : open_(value) {}
  size_t write(const void* data, size_t count) {
    if (!open_) return 0;
    StyledStorageStub::staged.append(static_cast<const char*>(data), count);
    return count;
  }
  bool close() {
    const bool wasOpen = open_;
    open_ = false;
    return wasOpen;
  }
  operator bool() const { return open_; }

 private:
  bool open_ = false;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  HalFile open(const char*, oflag_t flags) {
    if ((flags & O_TRUNC) != 0) StyledStorageStub::staged.clear();
    return HalFile(true);
  }
  bool remove(const char*) { return true; }
};

#define Storage HalStorage::getInstance()
