#pragma once

#include <Epub/converters/ImageToFramebufferDecoder.h>

class ImageDimsProbe {
 public:
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t*, size_t len) { return len; }
  bool getDimensions(ImageDimensions&) const { return false; }
};
