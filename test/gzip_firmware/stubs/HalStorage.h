#pragma once
// In-memory SD card: path -> bytes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

class HalFile {
 public:
  const std::string* data = nullptr;
  size_t pos = 0;
  int read(void* buf, size_t n) {
    if (!data) return -1;
    const size_t k = std::min(n, data->size() - pos);
    std::memcpy(buf, data->data() + pos, k);
    pos += k;
    return static_cast<int>(k);
  }
  size_t fileSize() { return data ? data->size() : 0; }
  bool seekSet(size_t p) {
    if (!data || p > data->size()) return false;
    pos = p;
    return true;
  }
  size_t position() const { return pos; }
  void close() { data = nullptr; }
  explicit operator bool() const { return data != nullptr; }
};

class HalStorage {
 public:
  std::map<std::string, std::string> files;
  bool openFileForRead(const char*, const char* path, HalFile& f) {
    auto it = files.find(path);
    if (it == files.end()) return false;
    f.data = &it->second;
    f.pos = 0;
    return true;
  }
};
extern HalStorage Storage;
