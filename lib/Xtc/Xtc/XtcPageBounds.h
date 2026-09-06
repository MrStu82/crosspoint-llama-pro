#pragma once
#include "XtcTypes.h"
#include <cstddef>
namespace xtc {
// Supported uncompressed page envelope. Non-byte-aligned XTH columns have
// no agreed encoding in this fork: reject rather than reinterpret grayscale.
inline size_t pageBitmapBytes(uint16_t w, uint16_t h, uint8_t depth) {
  if (!w || !h || w > DISPLAY_HEIGHT || h > DISPLAY_HEIGHT ||
      (depth != 1 && depth != 2) || (depth == 2 && h % 8)) return 0;
  return depth == 2 ? static_cast<size_t>(w) * (h / 8) * 2 : ((w + 7) / 8) * static_cast<size_t>(h);
}
inline bool validPageEntry(const PageTableEntry& e, uint8_t depth, uint64_t fileSize) {
  const size_t bytes = pageBitmapBytes(e.width, e.height, depth);
  return bytes && e.dataOffset <= fileSize && e.dataSize <= fileSize - e.dataOffset &&
         e.dataSize >= sizeof(XtgPageHeader) + bytes;
}
inline bool validPageHeader(const XtgPageHeader& h, const PageInfo& p) {
  const size_t bytes = pageBitmapBytes(p.width, p.height, p.bitDepth);
  return bytes && h.width == p.width && h.height == p.height && h.compression == 0 &&
         h.dataSize == bytes && p.size >= sizeof(XtgPageHeader) + bytes;
}
}  // namespace xtc
