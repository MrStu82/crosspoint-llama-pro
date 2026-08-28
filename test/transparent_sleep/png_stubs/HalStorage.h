#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class HalFile {
 public:
  int size() const { return 0; }
  int read(uint8_t*, int) { return 0; }
  int32_t seek(int32_t) { return 0; }
  size_t write(const void*, size_t size) { return size; }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }

 private:
  bool open_ = false;
};

struct StorageStub {
  bool openFileForRead(const char*, const std::string&, HalFile&) { return false; }
  bool openFileForWrite(const char*, const std::string&, HalFile&) { return false; }
  bool remove(const char*) { return true; }
};
inline StorageStub Storage;
