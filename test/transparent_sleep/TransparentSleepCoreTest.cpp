#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#define private public
#include "../../src/activities/boot_sleep/SleepActivity.cpp"
#undef private

namespace {

void put16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
  data[offset] = static_cast<uint8_t>(value);
  data[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void put32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
  for (int byte = 0; byte < 4; ++byte) data[offset + byte] = static_cast<uint8_t>(value >> (byte * 8));
}

std::vector<uint8_t> bmp(int32_t width, int32_t height, uint16_t bpp, const std::vector<uint8_t>& pixels) {
  std::vector<uint8_t> data(54 + pixels.size(), 0);
  data[0] = 'B';
  data[1] = 'M';
  put32(data, 2, static_cast<uint32_t>(data.size()));
  put32(data, 10, 54);
  put32(data, 14, 40);
  put32(data, 18, static_cast<uint32_t>(width));
  put32(data, 22, static_cast<uint32_t>(height));
  put16(data, 26, 1);
  put16(data, 28, bpp);
  put32(data, 30, 0);
  std::copy(pixels.begin(), pixels.end(), data.begin() + 54);
  return data;
}

void resetHarness() {
  Storage.clear();
  SETTINGS = CrossPointSettings{};
  APP_STATE = CrossPointState{};
  display = HalDisplay{};
  gpio = HalGPIO{};
  GUI = GuiStub{};
  Bitmap::lastDither = true;
}

TEST(TransparentSleepCore, BgraAlphaComposesAndPreservesPerPixel) {
  resetHarness();
  // Bottom-up BGRA: opaque black, fully transparent black, opaque white.
  HalFile file(bmp(3, 1, 32, {0, 0, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255}));
  GfxRenderer renderer(3, 1);

  ASSERT_EQ(tryRenderTransparentOverlayBmp(file, renderer, "fixture"), AlphaOverlayResult::Rendered);

  bool blackPainted = false;
  bool transparentTouched = false;
  bool whiteErased = false;
  for (const auto& draw : renderer.pixelDraws) {
    if (draw.mode != GfxRenderer::BW) continue;
    if (draw.x == 0 && draw.state) blackPainted = true;
    if (draw.x == 1) transparentTouched = true;
    if (draw.x == 2 && !draw.state) whiteErased = true;
  }
  EXPECT_TRUE(blackPainted);
  EXPECT_FALSE(transparentTouched);
  EXPECT_TRUE(whiteErased);
}

TEST(TransparentSleepCore, RegularBmpFallbackDisablesDithering) {
  resetHarness();
  HalFile file(bmp(1, 1, 24, {0, 0, 0, 0}));
  GfxRenderer renderer(8, 8);
  MappedInputManager input;
  SleepActivity activity(renderer, input);

  ASSERT_TRUE(activity.renderSleepOverlayFile(file, "regular.bmp"));
  EXPECT_FALSE(Bitmap::lastDither);
  EXPECT_GT(renderer.drawBitmapCalls, 0);
}

TEST(TransparentSleepCore, TransparentSleepPreservesVisibleNightFrameThenCleansDriverPolarity) {
  resetHarness();
  SETTINGS.sleepScreen = CrossPointSettings::TRANSPARENT_CUSTOM;
  display.setInverted(true);
  // Mixed alpha makes this a valid transparent overlay and avoids the authored
  // fallback screen's deliberate dark inversion.
  Storage.addFile("/sleep-overlay.bmp", bmp(2, 1, 32, {0, 0, 0, 255, 255, 255, 255, 0}));
  GfxRenderer renderer(32, 48);
  MappedInputManager input;
  SleepActivity activity(renderer, input);

  activity.onEnter();

  EXPECT_EQ(renderer.invertCalls, 1);  // materialized the visible Night Mode frame
  EXPECT_FALSE(display.isInverted());  // output driver is clean for retained sleep
  EXPECT_EQ(renderer.fontCache.releaseCalls, 1);
}

TEST(TransparentSleepCore, CacheReleasePrecedesMissingImageFallback) {
  resetHarness();
  SETTINGS.sleepScreen = CrossPointSettings::TRANSPARENT_CUSTOM;
  GfxRenderer renderer(32, 48);
  MappedInputManager input;
  SleepActivity activity(renderer, input);

  activity.onEnter();

  EXPECT_EQ(renderer.fontCache.releaseCalls, 1);
  ASSERT_FALSE(renderer.refreshes.empty());
  EXPECT_EQ(renderer.refreshes.back(), HalDisplay::FULL_REFRESH);
}

TEST(TransparentSleepCore, ExistingCustomDecodeAlsoReleasesSdCache) {
  resetHarness();
  SETTINGS.sleepScreen = CrossPointSettings::CUSTOM;
  GfxRenderer renderer(32, 48);
  MappedInputManager input;
  SleepActivity activity(renderer, input);

  activity.onEnter();

  EXPECT_EQ(renderer.fontCache.releaseCalls, 1);
  ASSERT_FALSE(renderer.refreshes.empty());
  EXPECT_EQ(renderer.refreshes.back(), HalDisplay::FULL_REFRESH);
}

TEST(TransparentSleepCore, CorruptRootImageFallsBackCleanly) {
  resetHarness();
  SETTINGS.sleepScreen = CrossPointSettings::TRANSPARENT_CUSTOM;
  Storage.addFile("/sleep-overlay.bmp", {'B', 'A', 'D'});
  GfxRenderer renderer(32, 48);
  MappedInputManager input;
  SleepActivity activity(renderer, input);

  activity.onEnter();

  EXPECT_EQ(renderer.fontCache.releaseCalls, 1);
  ASSERT_FALSE(renderer.refreshes.empty());
  EXPECT_EQ(renderer.refreshes.back(), HalDisplay::FULL_REFRESH);
}

TEST(TransparentSleepCore, HeaderBoundsAndTruncatedRowsFailWithoutOverflow) {
  resetHarness();
  OverlayBmpInfo info;

  HalFile tooWide(bmp(2049, 1, 32, {}));
  EXPECT_FALSE(parseOverlayBmpHeader(tooWide, info, false));

  HalFile impossibleHeight(bmp(1, std::numeric_limits<int32_t>::min(), 32, {}));
  EXPECT_FALSE(parseOverlayBmpHeader(impossibleHeight, info, false));

  HalFile truncated(bmp(2, 1, 32, {0, 0, 0, 128}));
  GfxRenderer renderer(8, 8);
  EXPECT_EQ(tryRenderTransparentOverlayBmp(truncated, renderer, "truncated"), AlphaOverlayResult::Error);
}

}  // namespace
