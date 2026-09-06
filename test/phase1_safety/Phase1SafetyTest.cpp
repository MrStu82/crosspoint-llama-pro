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

TEST(Phase1Safety, SegmentRangesRejectOverflowAndTruncation) {
  EXPECT_TRUE(safety_guards::rangeFits(32, 100, 132));
  EXPECT_TRUE(safety_guards::rangeFits(132, 0, 132));
  EXPECT_FALSE(safety_guards::rangeFits(32, 101, 132));
  EXPECT_FALSE(safety_guards::rangeFits(133, 0, 132));
  EXPECT_FALSE(safety_guards::rangeFits(32, SIZE_MAX - 16, 65536));
  EXPECT_FALSE(safety_guards::rangeFits(32, UINT32_MAX - 16, 65536));
}

#include "../../src/network/OtaVersion.h"
TEST(Phase1Safety, OtaVersionsAreInitializedAndAcceptDeliveredPrefix) {
  EXPECT_TRUE(ota_version::isNewer("v1.5.0-225-g425b02e", "v1.6.0"));
  EXPECT_FALSE(ota_version::isNewer("v1.5.0-225-g425b02e", "1.5.0"));
  EXPECT_FALSE(ota_version::isNewer("v1.5.0", "1.4.9"));
  EXPECT_TRUE(ota_version::isNewer("1.6.0-rc1", "1.6.0"));
  EXPECT_FALSE(ota_version::isNewer("1.6.0-rc1", "1.6.0-rc1"));
  for (const char* bad : {"", "v", "1.2", "x1.2.3", "1.2.3x", "4294967296.0.0", "1..3"}) {
    EXPECT_FALSE(ota_version::isNewer(bad, "9.0.0"));
    EXPECT_FALSE(ota_version::isNewer("1.0.0", bad));
  }
}
