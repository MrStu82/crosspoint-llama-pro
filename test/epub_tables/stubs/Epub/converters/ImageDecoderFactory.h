#pragma once

#include <Epub/converters/ImageToFramebufferDecoder.h>

#include <string>

class ImageDecoderFactory {
 public:
  static bool isFormatSupported(const std::string&) { return false; }
  static ImageToFramebufferDecoder* getDecoder(const std::string&) { return nullptr; }
};
