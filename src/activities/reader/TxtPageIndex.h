#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace txt_index {
constexpr uint32_t Magic = 0x54585449;
constexpr uint8_t Version = 4;
constexpr uint32_t MaxPages = 65536; // Bound even an SD-corrupted, large count.
struct Identity {
  uint32_t fileSize = 0;
  uint32_t layout = 0;
  std::array<uint8_t, 32> content{};
  std::array<uint8_t, 32> font{};
};
template <class File, class T> bool read(File& f, T& v) {
  return f.read(reinterpret_cast<uint8_t*>(&v), sizeof(v)) == static_cast<int>(sizeof(v));
}
// Transactional in memory: no partially read offsets escape on cache failure.
// Version 3 is deliberately rebuilt; book and progress are never removed.
template <class File> bool load(File& f, const Identity& expected, std::vector<size_t>& result) {
  constexpr size_t headerBytes = 4 + 1 + 4 + 4 + 32 + 32 + 4;
  if (f.size() < headerBytes) return false;
  uint32_t magic = 0, count = 0; uint8_t version = 0; Identity got;
  if (!read(f, magic) || magic != Magic || !read(f, version) || version != Version ||
      !read(f, got.fileSize) || !read(f, got.layout) || !read(f, got.content) || !read(f, got.font) || !read(f, count) ||
      got.fileSize != expected.fileSize || got.layout != expected.layout || got.content != expected.content || got.font != expected.font ||
      count == 0 || count > MaxPages || count > (expected.fileSize ? expected.fileSize : 1) ||
      count != (f.size() - headerBytes) / 4 || (f.size() - headerBytes) % 4) return false;
  std::vector<size_t> offsets;
  offsets.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t offset = 0;
    if (!read(f, offset) || (i == 0 && offset != 0) ||
        (i && offset <= offsets.back()) ||
        (offset >= expected.fileSize && !(count == 1 && offset == 0 && expected.fileSize == 0))) return false;
    offsets.push_back(offset);
  }
  result.swap(offsets);
  return true;
}
template <class File, class T> bool write(File& f, const T& v) {
  return f.write(reinterpret_cast<const uint8_t*>(&v), sizeof(v)) == sizeof(v);
}
template <class File> bool save(File& f, const Identity& id, const std::vector<size_t>& offsets) {
  const uint32_t count = offsets.size();
  if (!count || offsets.size() > MaxPages || !write(f, Magic) || !write(f, Version) ||
      !write(f, id.fileSize) || !write(f, id.layout) || !write(f, id.content) || !write(f, id.font) || !write(f, count)) return false;
  for (size_t offset : offsets) {
    if (offset > UINT32_MAX || !write(f, static_cast<uint32_t>(offset))) return false;
  }
  return true;
}
} // namespace txt_index
