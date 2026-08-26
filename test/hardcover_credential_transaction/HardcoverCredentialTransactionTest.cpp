#include <gtest/gtest.h>

#include <string>

#include "src/util/AtomicCredentialUpdate.h"

TEST(HardcoverCredentialTransaction, PersistenceFailureRestoresPreviousValue) {
  std::string memory = "hc_pat_previous";
  std::string durable = memory;

  const bool applied = AtomicCredentialUpdate::replace(memory, "hc_pat_replacement", [&] { return false; });

  EXPECT_FALSE(applied);
  EXPECT_EQ(memory, "hc_pat_previous");
  EXPECT_EQ(durable, "hc_pat_previous");
}

TEST(HardcoverCredentialTransaction, SuccessfulReplacementSurvivesReload) {
  std::string memory = "hc_pat_previous";
  std::string durable = memory;

  const bool applied = AtomicCredentialUpdate::replace(memory, "hc_pat_replacement", [&] {
    durable = memory;
    return true;
  });

  ASSERT_TRUE(applied);
  memory.clear();
  memory = durable;  // Settings-store reconstruction after reboot.
  EXPECT_EQ(memory, "hc_pat_replacement");
}

TEST(HardcoverCredentialTransaction, ForgetFailureAlsoRestoresPreviousValue) {
  std::string memory = "hc_pat_previous";
  const bool applied = AtomicCredentialUpdate::replace(memory, "", [] { return false; });
  EXPECT_FALSE(applied);
  EXPECT_EQ(memory, "hc_pat_previous");
}
