#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include "CrossPointState.h"

namespace {

TEST(TransparentSleepState, OverlayHistoryRoundTripsIndependently) {
  auto& source = CrossPointState::getInstance();
  JsonDocument emptyDocument;
  ASSERT_TRUE(source.fromJson(emptyDocument.as<JsonVariantConst>()));
  source.favoriteSleepImagePath = "/favorite.bmp";
  source.recentSleepPos = 0;
  source.recentSleepFill = 0;
  source.recentOverlaySleepPos = 0;
  source.recentOverlaySleepFill = 0;
  source.pushRecentSleep(11);
  source.pushRecentOverlaySleep(21);
  source.pushRecentOverlaySleep(22);

  JsonDocument document;
  source.toJson(document);

  source.favoriteSleepImagePath.clear();
  source.recentSleepPos = 0;
  source.recentSleepFill = 0;
  source.recentOverlaySleepPos = 0;
  source.recentOverlaySleepFill = 0;
  ASSERT_TRUE(source.fromJson(document.as<JsonVariantConst>()));

  EXPECT_EQ(source.favoriteSleepImagePath, "/favorite.bmp");
  EXPECT_TRUE(source.isRecentSleep(11, CrossPointState::SLEEP_RECENT_COUNT));
  EXPECT_FALSE(source.isRecentSleep(21, CrossPointState::SLEEP_RECENT_COUNT));
  EXPECT_TRUE(source.isRecentOverlaySleep(22, 1));
  EXPECT_TRUE(source.isRecentOverlaySleep(21, 2));
  EXPECT_FALSE(source.isRecentOverlaySleep(11, CrossPointState::SLEEP_RECENT_COUNT));
}

TEST(TransparentSleepState, CorruptPersistedBoundsAreClamped) {
  JsonDocument document;
  JsonArray values = document["recentOverlaySleepImages"].to<JsonArray>();
  values.add(7);
  values.add(8);
  document["recentOverlaySleepPos"] = 255;
  document["recentOverlaySleepFill"] = 255;

  auto& state = CrossPointState::getInstance();
  ASSERT_TRUE(state.fromJson(document.as<JsonVariantConst>()));

  EXPECT_LT(state.recentOverlaySleepPos, CrossPointState::SLEEP_RECENT_COUNT);
  EXPECT_EQ(state.recentOverlaySleepFill, 2);
}

TEST(TransparentSleepState, SplashlessWakeFlagPersistsAndDefaultsSafe) {
  auto& state = CrossPointState::getInstance();
  JsonDocument firstBoot;
  ASSERT_TRUE(state.fromJson(firstBoot.as<JsonVariantConst>()));
  EXPECT_TRUE(state.showBootScreen);

  state.showBootScreen = false;
  JsonDocument sleeping;
  state.toJson(sleeping);
  state.showBootScreen = true;
  ASSERT_TRUE(state.fromJson(sleeping.as<JsonVariantConst>()));
  EXPECT_FALSE(state.showBootScreen);

  state.showBootScreen = true;
  JsonDocument rearmed;
  state.toJson(rearmed);
  state.showBootScreen = false;
  ASSERT_TRUE(state.fromJson(rearmed.as<JsonVariantConst>()));
  EXPECT_TRUE(state.showBootScreen);
}

}  // namespace
