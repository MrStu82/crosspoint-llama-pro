#include <gtest/gtest.h>
#include "SafetyGuards.h"
TEST(Phase1Safety, HoldsConfiguredX4LatchButNeverBusOrC3PowerOffPin) {
  EXPECT_TRUE(safety_guards::shouldHoldPowerLatch(1, 13, false));
  EXPECT_FALSE(safety_guards::shouldHoldPowerLatch(-1, 13, false));
  EXPECT_FALSE(safety_guards::shouldHoldPowerLatch(13, 13, false));
  EXPECT_FALSE(safety_guards::shouldHoldPowerLatch(1, 13, true));
}
TEST(Phase1Safety, RejectsWrongChipButPermitsUnavailableProbe) {
  EXPECT_TRUE(safety_guards::imageChipMatchesDevice(9, 9));
  EXPECT_FALSE(safety_guards::imageChipMatchesDevice(2, 9));
  EXPECT_TRUE(safety_guards::imageChipMatchesDevice(2, 0xFFFF));
}
