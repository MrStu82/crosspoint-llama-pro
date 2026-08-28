#include <gtest/gtest.h>

#include <Arduino.h>
#include <BoardConfig.h>
#include <FrontlightManager.h>

namespace {

constexpr uint32_t FULL_10_BIT = 1023;

uint32_t logicalTotal() { return TestPwm::duty[0] + TestPwm::duty[1]; }

FrontlightManager begunX4Pro(uint8_t warmPercent = 50) {
  BoardConfig::ACTIVE = {BoardConfig::Board::XteinkX4Pro, {8, 10000, 10, true, 9}};
  TestPwm::reset();
  FrontlightManager manager;
  EXPECT_TRUE(manager.begin());
  manager.setColorTemperature(warmPercent);
  return manager;
}

TEST(X4ProFrontlight, ZeroIsOffAndOnePercentUsesTheMinimumDuty) {
  auto manager = begunX4Pro();
  manager.setBrightness(0);
  EXPECT_EQ(logicalTotal(), 0u);

  manager.setBrightness(1);
  EXPECT_EQ(logicalTotal(), 1u);
}

TEST(X4ProFrontlight, EveryFinePercentageStepIsStrictlyMonotonic) {
  auto manager = begunX4Pro(37);
  uint32_t previous = 0;
  for (uint8_t percent = 1; percent <= 100; ++percent) {
    manager.setBrightness(percent);
    const uint32_t current = logicalTotal();
    EXPECT_GT(current, previous) << "percent=" << static_cast<int>(percent);
    previous = current;
  }
  EXPECT_EQ(previous, FULL_10_BIT);
}

TEST(X4ProFrontlight, WarmAndCoolEndpointsPreserveCalibratedTotal) {
  auto manager = begunX4Pro(0);
  manager.setBrightness(42);
  const uint32_t total = logicalTotal();
  EXPECT_GT(total, 0u);
  EXPECT_EQ(TestPwm::duty[0], total);
  EXPECT_EQ(TestPwm::duty[1], 0u);

  manager.setColorTemperature(100);
  EXPECT_EQ(logicalTotal(), total);
  EXPECT_EQ(TestPwm::duty[0], 0u);
  EXPECT_EQ(TestPwm::duty[1], total);

  manager.setColorTemperature(50);
  EXPECT_EQ(logicalTotal(), total);
  EXPECT_LE(TestPwm::duty[0] > TestPwm::duty[1] ? TestPwm::duty[0] - TestPwm::duty[1]
                                               : TestPwm::duty[1] - TestPwm::duty[0],
            1u);
}

TEST(X4ProFrontlight, LoadedValuesAndLastBrightnessSurviveOffOn) {
  constexpr uint8_t savedBrightness = 17;
  constexpr uint8_t savedWarmth = 73;
  auto manager = begunX4Pro();
  manager.setColorTemperature(savedWarmth);
  manager.setBrightness(savedBrightness);
  const auto savedDuties = TestPwm::duty;

  EXPECT_EQ(manager.brightness(), savedBrightness);
  EXPECT_EQ(manager.colorTemperature(), savedWarmth);
  manager.off();
  EXPECT_EQ(manager.brightness(), 0u);
  EXPECT_EQ(logicalTotal(), 0u);
  EXPECT_EQ(manager.colorTemperature(), savedWarmth);
  manager.on();
  EXPECT_EQ(manager.brightness(), savedBrightness);
  EXPECT_EQ(manager.colorTemperature(), savedWarmth);
  EXPECT_EQ(TestPwm::duty, savedDuties);
}

TEST(OtherBoardFrontlight, ExistingLinearDualChannelMappingIsUnchanged) {
  BoardConfig::ACTIVE = {BoardConfig::Board::Other, {4, 5000, 10, true, 5}};
  TestPwm::reset();
  FrontlightManager manager;
  ASSERT_TRUE(manager.begin());

  manager.setColorTemperature(37);
  manager.setBrightness(17);
  const uint32_t coolPercent = (17u * (100u - 37u)) / 100u;
  const uint32_t warmPercent = (17u * 37u) / 100u;
  EXPECT_EQ(TestPwm::duty[0], (coolPercent * FULL_10_BIT) / 100u);
  EXPECT_EQ(TestPwm::duty[1], (warmPercent * FULL_10_BIT) / 100u);
}

TEST(OtherBoardFrontlight, ExistingActiveLowSingleChannelRangeIsUnchanged) {
  BoardConfig::ACTIVE = {BoardConfig::Board::Other, {4, 5000, 8, false, BoardConfig::PIN_UNASSIGNED}};
  TestPwm::reset();
  FrontlightManager manager;
  ASSERT_TRUE(manager.begin());

  manager.setBrightness(0);
  EXPECT_EQ(TestPwm::duty[0], 255u);
  manager.setBrightness(37);
  EXPECT_EQ(TestPwm::duty[0], 255u - (37u * 255u) / 100u);
  manager.setBrightness(100);
  EXPECT_EQ(TestPwm::duty[0], 0u);
}

}  // namespace
