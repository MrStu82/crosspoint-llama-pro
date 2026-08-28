#pragma once

#include <Arduino.h>

#include <cstdint>
#include <vector>

class GfxRenderer {
 public:
  enum RenderMode { BW, GRAYSCALE_MSB, GRAYSCALE_LSB };
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  GfxRenderer(int width, int height)
      : width_(width), height_(height), stride_((width + 7) / 8), framebuffer_(stride_ * height, 0xFF) {}

  int getScreenWidth() const { return width_; }
  int getScreenHeight() const { return height_; }
  uint8_t* getWriteTarget() { return framebuffer_.data(); }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return height_; }
  RenderMode getRenderMode() const { return mode_; }
  uint16_t getDisplayWidthBytes() const { return static_cast<uint16_t>(stride_); }
  int getDisplayWidth() const { return width_; }
  int getDisplayHeight() const { return height_; }
  Orientation getOrientation() const { return LandscapeCounterClockwise; }
  void setRenderMode(RenderMode mode) { mode_ = mode; }

  std::vector<uint8_t>& framebuffer() { return framebuffer_; }
  const std::vector<uint8_t>& framebuffer() const { return framebuffer_; }

 private:
  int width_;
  int height_;
  int stride_;
  RenderMode mode_ = BW;
  std::vector<uint8_t> framebuffer_;
};
