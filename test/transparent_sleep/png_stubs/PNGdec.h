#pragma once

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

inline constexpr int PNG_SUCCESS = 0;
inline constexpr int PNG_ERROR = -1;
inline constexpr int PNG_PIXEL_GRAYSCALE = 0;
inline constexpr int PNG_PIXEL_TRUECOLOR = 2;
inline constexpr int PNG_PIXEL_INDEXED = 3;
inline constexpr int PNG_PIXEL_GRAY_ALPHA = 4;
inline constexpr int PNG_PIXEL_TRUECOLOR_ALPHA = 6;
inline constexpr int PNG_MAX_BUFFERED_PIXELS = 65536;

struct PNGFILE {
  void* fHandle = nullptr;
};

struct PNGDRAW {
  void* pUser = nullptr;
  int y = 0;
  uint8_t* pPixels = nullptr;
  int iPixelType = PNG_PIXEL_TRUECOLOR_ALPHA;
  int iBpp = 8;
  uint8_t* pPalette = nullptr;
  int iHasAlpha = 1;
};

class PNG {
 public:
  using DrawCallback = int (*)(PNGDRAW*);

  template <typename Open, typename Close, typename Read, typename Seek>
  int open(const char* path, Open, Close, Read, Seek, DrawCallback draw) {
    draw_ = draw;
    std::ifstream input(path, std::ios::binary);
    bytes_.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (bytes_.size() < 33 || std::memcmp(bytes_.data(), "\x89PNG\r\n\x1a\n", 8) != 0) return PNG_ERROR;
    width_ = readBe32(16);
    height_ = readBe32(20);
    bpp_ = bytes_[24];
    pixelType_ = bytes_[25];
    return PNG_SUCCESS;
  }

  void close() {}
  int getWidth() const { return static_cast<int>(width_); }
  int getHeight() const { return static_cast<int>(height_); }
  int getPixelType() const { return pixelType_; }
  int getBpp() const { return bpp_; }
  uint32_t getTransparentColor() const { return 0; }

  int decode(void* user, int) {
    if (!draw_ || pixelType_ != PNG_PIXEL_TRUECOLOR_ALPHA || bpp_ != 8) return PNG_ERROR;
    std::vector<uint8_t> compressed;
    for (size_t offset = 8; offset + 12 <= bytes_.size();) {
      const uint32_t length = readBe32(offset);
      if (offset + 12u + length > bytes_.size()) return PNG_ERROR;
      const char* type = reinterpret_cast<const char*>(bytes_.data() + offset + 4);
      if (std::memcmp(type, "IDAT", 4) == 0) {
        compressed.insert(compressed.end(), bytes_.begin() + offset + 8, bytes_.begin() + offset + 8 + length);
      }
      if (std::memcmp(type, "IEND", 4) == 0) break;
      offset += 12u + length;
    }

    const size_t rowBytes = static_cast<size_t>(width_) * 4u;
    std::vector<uint8_t> raw(static_cast<size_t>(height_) * (rowBytes + 1u));
    uLongf rawSize = raw.size();
    if (uncompress(raw.data(), &rawSize, compressed.data(), compressed.size()) != Z_OK || rawSize != raw.size()) {
      return PNG_ERROR;
    }
    for (uint32_t y = 0; y < height_; ++y) {
      uint8_t* row = raw.data() + static_cast<size_t>(y) * (rowBytes + 1u);
      if (row[0] != 0) return PNG_ERROR;
      PNGDRAW draw;
      draw.pUser = user;
      draw.y = static_cast<int>(y);
      draw.pPixels = row + 1;
      if (!draw_(&draw)) return PNG_ERROR;
    }
    return PNG_SUCCESS;
  }

 private:
  uint32_t readBe32(size_t offset) const {
    return (static_cast<uint32_t>(bytes_[offset]) << 24) | (static_cast<uint32_t>(bytes_[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes_[offset + 2]) << 8) | static_cast<uint32_t>(bytes_[offset + 3]);
  }

  std::vector<uint8_t> bytes_;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  int bpp_ = 8;
  int pixelType_ = PNG_PIXEL_TRUECOLOR_ALPHA;
  DrawCallback draw_ = nullptr;
};
