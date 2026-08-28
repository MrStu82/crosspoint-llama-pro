#pragma once

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <cstdint>
#include <string>

struct ImageDimensions {
  int16_t width = 0;
  int16_t height = 0;
};
struct RenderConfig {
  int x = 0;
  int y = 0;
  int maxWidth = 0;
  int maxHeight = 0;
  bool useGrayscale = true;
  bool useDithering = true;
  bool performanceMode = false;
  bool useExactDimensions = false;
  float sourceCropX = 0;
  float sourceCropY = 0;
  bool preserveAlpha = false;
};
class PngToFramebufferConverter {
 public:
  static bool getDimensionsStatic(const std::string& path, ImageDimensions& out) {
    if (!Storage.exists(path.c_str())) return false;
    out = {1, 1};
    return true;
  }
  bool decodeToFramebuffer(const std::string&, GfxRenderer&, const RenderConfig& config) {
    lastConfig = config;
    return true;
  }
  static inline RenderConfig lastConfig;
};
