#pragma once

#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <string>

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(FILE* value) : fp_(value) {}
  ~HalFile() { close(); }
  HalFile(HalFile&& other) noexcept : fp_(other.fp_) { other.fp_ = nullptr; }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this != &other) {
      close();
      fp_ = other.fp_;
      other.fp_ = nullptr;
    }
    return *this;
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  int read(void* data, size_t count) { return fp_ ? static_cast<int>(fread(data, 1, count, fp_)) : -1; }
  int read() { return fp_ ? fgetc(fp_) : -1; }
  size_t write(const void* data, size_t count) { return fp_ ? fwrite(data, 1, count, fp_) : 0; }
  size_t write(const uint8_t* data, size_t count) { return write(static_cast<const void*>(data), count); }
  size_t write(uint8_t value) { return write(&value, 1); }
  bool seek(size_t pos) { return fp_ && fseek(fp_, static_cast<long>(pos), SEEK_SET) == 0; }
  bool seekSet(size_t pos) { return seek(pos); }
  bool seekCur(int64_t offset) { return fp_ && fseek(fp_, static_cast<long>(offset), SEEK_CUR) == 0; }
  size_t position() const { return fp_ ? static_cast<size_t>(ftell(fp_)) : 0; }
  size_t size() {
    if (!fp_) return 0;
    const long current = ftell(fp_);
    fseek(fp_, 0, SEEK_END);
    const long end = ftell(fp_);
    fseek(fp_, current, SEEK_SET);
    return static_cast<size_t>(end);
  }
  size_t fileSize() { return size(); }
  int available() { return fp_ ? static_cast<int>(size() - position()) : 0; }
  void flush() {
    if (fp_) fflush(fp_);
  }
  bool close() {
    if (!fp_) return false;
    const bool ok = fclose(fp_) == 0;
    fp_ = nullptr;
    return ok;
  }
  bool isOpen() const { return fp_ != nullptr; }
  operator bool() const { return isOpen(); }

 private:
  FILE* fp_ = nullptr;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    file = HalFile(fopen(path.c_str(), "rb"));
    return static_cast<bool>(file);
  }
  bool openFileForRead(const char* module, const char* path, HalFile& file) {
    return openFileForRead(module, std::string(path), file);
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    file = HalFile(fopen(path.c_str(), "wb"));
    return static_cast<bool>(file);
  }
  bool exists(const char* path) { return path && fopenExists(path); }
  bool rename(const char* from, const char* to) { return std::rename(from, to) == 0; }
  bool remove(const char* path) { return std::remove(path) == 0; }

 private:
  static bool fopenExists(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
  }
};

#define Storage HalStorage::getInstance()
