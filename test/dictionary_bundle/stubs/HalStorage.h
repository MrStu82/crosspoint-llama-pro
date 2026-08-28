#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

using oflag_t = uint32_t;
constexpr oflag_t O_RDONLY = 0;
constexpr oflag_t O_WRITE = 1u << 0;
constexpr oflag_t O_CREAT = 1u << 1;
constexpr oflag_t O_TRUNC = 1u << 2;

class HalFile {
 public:
  HalFile() = default;
  explicit HalFile(FILE* value) : fp(value) {}
  ~HalFile() { close(); }
  HalFile(HalFile&& other) noexcept : fp(other.fp) { other.fp = nullptr; }
  HalFile& operator=(HalFile&& other) noexcept {
    if (this != &other) {
      close();
      fp = other.fp;
      other.fp = nullptr;
    }
    return *this;
  }
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  int read(void* buf, size_t count) { return fp ? static_cast<int>(fread(buf, 1, count, fp)) : -1; }
  int read() { return fp ? fgetc(fp) : -1; }
  size_t write(const void* buf, size_t count) { return fp ? fwrite(buf, 1, count, fp) : 0; }
  bool seek(size_t pos) { return fp && fseek(fp, static_cast<long>(pos), SEEK_SET) == 0; }
  bool seekSet(size_t pos) { return seek(pos); }
  size_t position() const { return fp ? static_cast<size_t>(ftell(fp)) : 0; }
  size_t fileSize() {
    if (!fp) return 0;
    const long current = ftell(fp);
    fseek(fp, 0, SEEK_END);
    const long end = ftell(fp);
    fseek(fp, current, SEEK_SET);
    return static_cast<size_t>(end);
  }
  bool close() {
    if (!fp) return false;
    const bool ok = fclose(fp) == 0;
    fp = nullptr;
    return ok;
  }
  operator bool() const { return fp != nullptr; }

 private:
  FILE* fp = nullptr;
};

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  HalFile open(const char* path, oflag_t flags = O_RDONLY) {
    const std::filesystem::path full = root / pathWithoutLeadingSlash(path);
    if ((flags & O_CREAT) != 0) std::filesystem::create_directories(full.parent_path());
    const char* mode = (flags & O_WRITE) != 0 ? ((flags & O_TRUNC) != 0 ? "wb+" : "rb+") : "rb";
    return HalFile(fopen(full.string().c_str(), mode));
  }

  bool openFileForRead(const char*, const char* path, HalFile& file) {
    file = open(path, O_RDONLY);
    return static_cast<bool>(file);
  }
  bool openFileForRead(const char* module, const std::string& path, HalFile& file) {
    return openFileForRead(module, path.c_str(), file);
  }
  bool openFileForWrite(const char*, const char* path, HalFile& file) {
    file = open(path, O_WRITE | O_CREAT | O_TRUNC);
    return static_cast<bool>(file);
  }
  bool openFileForWrite(const char* module, const std::string& path, HalFile& file) {
    return openFileForWrite(module, path.c_str(), file);
  }
  bool exists(const char* path) const { return std::filesystem::exists(root / pathWithoutLeadingSlash(path)); }
  bool remove(const char* path) { return std::filesystem::remove(root / pathWithoutLeadingSlash(path)); }

  std::filesystem::path root;

 private:
  static std::string pathWithoutLeadingSlash(const char* path) {
    return path && path[0] == '/' ? std::string(path + 1) : std::string(path ? path : "");
  }
};

#define Storage HalStorage::getInstance()
