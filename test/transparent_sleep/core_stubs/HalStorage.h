#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(std::vector<uint8_t> bytes, std::string name = {})
      : bytes_(std::make_shared<std::vector<uint8_t>>(std::move(bytes))), name_(std::move(name)), open_(true) {}

  explicit operator bool() const { return open_; }
  bool isOpen() const { return open_; }
  bool isDirectory() const { return directory_; }
  int read() {
    if (!open_ || !bytes_ || pos_ >= bytes_->size()) return -1;
    return (*bytes_)[pos_++];
  }
  int read(uint8_t* out, size_t count) {
    if (!open_ || !bytes_) return 0;
    const size_t available = std::min(count, bytes_->size() - std::min(pos_, bytes_->size()));
    if (available) std::memcpy(out, bytes_->data() + pos_, available);
    pos_ += available;
    return static_cast<int>(available);
  }
  bool seek(uint32_t position) {
    if (!open_ || !bytes_ || position > bytes_->size()) return false;
    pos_ = position;
    return true;
  }
  bool seekCur(int32_t amount) { return amount >= 0 && seek(static_cast<uint32_t>(pos_ + amount)); }
  size_t size() const { return bytes_ ? bytes_->size() : 0; }
  size_t write(const void*, size_t count) { return count; }
  void close() { open_ = false; }
  void rewindDirectory() { dirIndex_ = 0; }
  HalFile openNextFile() { return {}; }
  void getName(char* out, size_t size) const {
    if (size == 0) return;
    std::strncpy(out, name_.c_str(), size - 1);
    out[size - 1] = '\0';
  }

 private:
  std::shared_ptr<std::vector<uint8_t>> bytes_;
  std::string name_;
  size_t pos_ = 0;
  size_t dirIndex_ = 0;
  bool open_ = false;
  bool directory_ = false;
};

struct StorageStub {
  std::map<std::string, std::vector<uint8_t>> files;
  void clear() { files.clear(); }
  void addFile(std::string path, std::vector<uint8_t> data) { files[std::move(path)] = std::move(data); }
  bool exists(const char* path) const { return files.contains(path); }
  HalFile open(const char*) { return {}; }
  bool openFileForRead(const char*, const std::string& path, HalFile& out) const {
    const auto it = files.find(path);
    if (it == files.end()) return false;
    out = HalFile(it->second, path);
    return true;
  }
};

inline StorageStub Storage;
