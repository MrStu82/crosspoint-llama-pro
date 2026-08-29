#include <gtest/gtest.h>

#include "src/activities/util/TransientPasswordReveal.h"
#include "src/components/ControlCenterModel.h"

namespace {
using namespace ControlCenterModel;

TEST(ControlCenterLayout, ApprovedPortraitGeometryIsExactAndAdditive) {
  const Layout l = layout(480, 800);
  EXPECT_EQ(l.sheetHeight, 372);
  EXPECT_EQ(l.brightnessCaption.x, 16);
  EXPECT_EQ(l.brightnessCaption.y, 18);
  EXPECT_EQ(l.brightnessCaption.width, 448);
  EXPECT_EQ(l.brightness.minus.x, 16);
  EXPECT_EQ(l.brightness.minus.y, 50);
  EXPECT_EQ(l.brightness.minus.width, 56);
  EXPECT_EQ(l.brightness.track.x, 72);
  EXPECT_EQ(l.brightness.track.width, 280);
  EXPECT_EQ(l.brightness.plus.x, 352);
  EXPECT_EQ(l.brightness.toggle.x, 408);

  EXPECT_EQ(l.warmthCaption.y, 114);
  EXPECT_EQ(l.warmth.minus.y, 146);
  EXPECT_EQ(l.warmth.track.x, 72);
  EXPECT_EQ(l.warmth.track.width, 336);
  EXPECT_EQ(l.warmth.plus.x, 408);

  EXPECT_EQ(l.tiles[0].x, 16);
  EXPECT_EQ(l.tiles[0].y, 210);
  EXPECT_EQ(l.tiles[0].width, 216);
  EXPECT_EQ(l.tiles[0].height, 56);
  EXPECT_EQ(l.tiles[1].x, 248);
  EXPECT_EQ(l.tiles[2].y, 282);
  EXPECT_EQ(l.grabber.x, 212);
  EXPECT_EQ(l.grabber.y, 358);
}

TEST(ControlCenterLayout, EveryExplicitControlIsFingerSizedAndWithinTheSheet) {
  const Layout l = layout(480, 800);
  const Rect controls[] = {l.brightness.minus, l.brightness.track, l.brightness.plus, l.brightness.toggle,
                           l.warmth.minus,     l.warmth.track,     l.warmth.plus,     l.tiles[0],
                           l.tiles[1],         l.tiles[2],         l.tiles[3]};
  for (const Rect& r : controls) {
    EXPECT_GE(r.height, 56);
    EXPECT_GE(r.width, 56);
    EXPECT_GE(r.x, 0);
    EXPECT_GE(r.y, 0);
    EXPECT_LE(r.x + r.width, 480);
    EXPECT_LE(r.y + r.height, l.sheetHeight);
  }
}

TEST(ControlCenterValues, BrightnessIsExactlyOneToOneHundred) {
  EXPECT_EQ(clampBrightness(-5), 1);
  EXPECT_EQ(clampBrightness(0), 1);
  EXPECT_EQ(clampBrightness(1), 1);
  EXPECT_EQ(clampBrightness(100), 100);
  EXPECT_EQ(clampBrightness(101), 100);

  const Rect track{86, 42, 252, 56};
  EXPECT_EQ(valueFromTrack(track.x, track, kBrightnessMin), 1);
  EXPECT_EQ(valueFromTrack(track.x + track.width - 1, track, kBrightnessMin), 100);
  int previous = 0;
  for (int x = track.x; x < track.x + track.width; ++x) {
    const int value = valueFromTrack(x, track, kBrightnessMin);
    EXPECT_GE(value, previous);
    EXPECT_GE(value, 1);
    EXPECT_LE(value, 100);
    previous = value;
  }
}

TEST(ControlCenterValues, WarmthPreservesFullCoolZeroAndFullWarmHundred) {
  EXPECT_EQ(clampWarmth(-1), 0);
  EXPECT_EQ(clampWarmth(0), 0);
  EXPECT_EQ(clampWarmth(100), 100);
  EXPECT_EQ(clampWarmth(101), 100);

  const Rect track{86, 158, 308, 56};
  EXPECT_EQ(valueFromTrack(track.x, track, kWarmthMin), 0);
  EXPECT_EQ(valueFromTrack(track.x + track.width - 1, track, kWarmthMin), 100);
  int previous = -1;
  for (int x = track.x; x < track.x + track.width; ++x) {
    const int value = valueFromTrack(x, track, kWarmthMin);
    EXPECT_GE(value, previous);
    EXPECT_GE(value, 0);
    EXPECT_LE(value, 100);
    previous = value;
  }
}

TEST(ControlCenterTiles, ActiveTouchOnUsesBlackFillAndWhiteText) {
  const TileInk active = tileInk(3, false, true);
  EXPECT_TRUE(active.blackFill);
  EXPECT_FALSE(active.blackText);

  const TileInk inactive = tileInk(3, false, false);
  EXPECT_FALSE(inactive.blackFill);
  EXPECT_TRUE(inactive.blackText);

  // Other tile selection remains independent of the touch state.
  EXPECT_TRUE(tileInk(0, true, false).blackFill);
  EXPECT_FALSE(tileInk(1, true, true).blackFill);
  EXPECT_FALSE(tileInk(2, true, true).blackFill);
}

TEST(ControlCenterValues, LegacyBrightnessMigratesWithoutTurningOnAnOffDevice) {
  const auto oldOff = migrateFrontlightState(false, 0, 1, 0);
  EXPECT_EQ(oldOff.brightness, 1);
  EXPECT_EQ(oldOff.on, 0);

  const auto oldOn = migrateFrontlightState(false, 37, 37, 0);
  EXPECT_EQ(oldOn.brightness, 37);
  EXPECT_EQ(oldOn.on, 1);

  const auto explicitOff = migrateFrontlightState(true, 37, 37, 0);
  EXPECT_EQ(explicitOff.brightness, 37);
  EXPECT_EQ(explicitOff.on, 0);
}

TEST(TransientPasswordReveal, ReleaseCancelFocusAndPageExitAllRemask) {
  TransientPasswordReveal reveal;
  EXPECT_FALSE(reveal.visible());

  EXPECT_TRUE(reveal.begin());
  EXPECT_TRUE(reveal.visible());
  EXPECT_TRUE(reveal.reset());  // pointer release
  EXPECT_FALSE(reveal.visible());

  EXPECT_TRUE(reveal.begin());
  EXPECT_TRUE(reveal.reset());  // pointer cancel / leaves target
  EXPECT_FALSE(reveal.visible());

  EXPECT_TRUE(reveal.begin());
  EXPECT_TRUE(reveal.reset());  // focus loss
  EXPECT_FALSE(reveal.visible());

  EXPECT_TRUE(reveal.begin());
  EXPECT_TRUE(reveal.reset());  // Activity::onExit / page change
  EXPECT_FALSE(reveal.visible());
  EXPECT_FALSE(reveal.reset());
}

TEST(TransientPasswordReveal, StateMachineCannotOwnPlaintext) {
  // The reveal model is a boolean permission only; the credential remains in
  // KeyboardEntryActivity and is never copied into persistence or logging.
  EXPECT_EQ(sizeof(TransientPasswordReveal), sizeof(bool));
}

}  // namespace
