#pragma once

#include <cstdint>
#include <string>

class GfxRenderer;

struct ImageDimensions {
  int16_t width;
  int16_t height;
};

class ImageToFramebufferDecoder {
 public:
  virtual ~ImageToFramebufferDecoder() = default;
  virtual bool getDimensions(const std::string&, ImageDimensions&) const { return false; }
};
