#include <gtest/gtest.h>

#include <Epub/converters/PngToFramebufferConverter.h>
#include <GfxRenderer.h>

#include <cstdint>
#include <string>

namespace {

std::string fixture(const char* name) { return std::string(TRANSPARENT_SLEEP_FIXTURE_DIR) + "/" + name; }

bool bit(const GfxRenderer& renderer, int x) { return (renderer.framebuffer()[0] & (0x80u >> x)) != 0; }

TEST(TransparentPng, AlphaCompositionPreservesOrReplacesExistingPixels) {
  GfxRenderer renderer(4, 1);
  // Existing pixels: white, black, white, black.
  renderer.framebuffer()[0] = 0b10100000;

  RenderConfig config;
  config.x = 0;
  config.y = 0;
  config.maxWidth = 4;
  config.maxHeight = 1;
  config.useDithering = false;
  config.preserveAlpha = true;

  PngToFramebufferConverter converter;
  ASSERT_TRUE(converter.decodeToFramebuffer(fixture("alpha_rgba.png"), renderer, config));

  EXPECT_FALSE(bit(renderer, 0));  // opaque black replaces white
  EXPECT_TRUE(bit(renderer, 1));   // opaque white erases black
  EXPECT_TRUE(bit(renderer, 2));   // alpha 0 preserves white
  EXPECT_FALSE(bit(renderer, 3));  // alpha 128 is below this Bayer cell; preserves black
}

TEST(TransparentPng, BoundsAreRejectedBeforeDecodeOrNarrowing) {
  ImageDimensions dimensions{};
  EXPECT_FALSE(PngToFramebufferConverter::getDimensionsStatic(fixture("oversize_header.png"), dimensions));

  GfxRenderer renderer(4, 1);
  RenderConfig config;
  config.x = 0;
  config.y = 0;
  config.maxWidth = 4;
  config.maxHeight = 1;
  config.preserveAlpha = true;
  PngToFramebufferConverter converter;
  EXPECT_FALSE(converter.decodeToFramebuffer(fixture("oversize_header.png"), renderer, config));
}

}  // namespace
