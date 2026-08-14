#include "StatsManager.h"

#include <string.h>

#include <array>

#include <gtest/gtest.h>

namespace {

// Builds a byte-exact legacy (v1, no version byte, 7-slot) stats.bin fixture: 5 populated
// days out of 7 slots, the other 2 left as empty (date=0) slots, plus non-zero scalar
// fields so a naive all-zero pass can't accidentally look correct.
GlobalStatsV1 makeLegacyFixture() {
  GlobalStatsV1 legacy{};
  legacy.totalPagesRead = 1234;
  legacy.booksOpened = 12;
  legacy.totalReadingTimeSeconds = 56789;
  legacy.readingTimeTodaySeconds = 300;
  legacy.pagesReadToday = 4;
  legacy.booksFinished = 3;
  legacy.lastActiveDate = 20260808;

  const int dates[5] = {20260801, 20260802, 20260804, 20260805, 20260807};
  const uint16_t minutes[5] = {12, 45, 30, 5, 60};
  for (int i = 0; i < 5; i++) {
    legacy.dailyHistory[i].date = dates[i];
    legacy.dailyHistory[i].minutes = minutes[i];
  }
  // Slots 5 and 6 stay zeroed (empty), matching a real ring buffer that hasn't wrapped yet.
  legacy.dailyHistoryWriteIndex = 5;  // Irrelevant to migration — always overridden to 7.
  return legacy;
}

std::array<uint8_t, sizeof(GlobalStatsV1)> legacyFixtureBytes() {
  GlobalStatsV1 legacy = makeLegacyFixture();
  std::array<uint8_t, sizeof(GlobalStatsV1)> buf{};
  memcpy(buf.data(), &legacy, sizeof(GlobalStatsV1));
  return buf;
}

}  // namespace

TEST(StatsMigration, MigratesLegacyV1IntoCurrentFormat) {
  auto buf = legacyFixtureBytes();
  GlobalStats out{};
  bool needsSave = false;

  ASSERT_TRUE(migrateStatsBlob(buf.data(), buf.size(), out, needsSave));
  EXPECT_TRUE(needsSave);

  EXPECT_EQ(out.totalPagesRead, 1234u);
  EXPECT_EQ(out.booksOpened, 12u);
  EXPECT_EQ(out.totalReadingTimeSeconds, 56789u);
  EXPECT_EQ(out.readingTimeTodaySeconds, 300u);
  EXPECT_EQ(out.pagesReadToday, 4u);
  EXPECT_EQ(out.booksFinished, 3u);
  EXPECT_EQ(out.lastActiveDate, 20260808);

  const int expectedDates[7] = {20260801, 20260802, 20260804, 20260805, 20260807, 0, 0};
  const uint16_t expectedMinutes[7] = {12, 45, 30, 5, 60, 0, 0};
  for (int i = 0; i < 7; i++) {
    EXPECT_EQ(out.dailyHistory[i].date, expectedDates[i]) << "slot " << i;
    EXPECT_EQ(out.dailyHistory[i].minutes, expectedMinutes[i]) << "slot " << i;
  }

  // The remaining 77 slots (7..83) must be untouched/empty — no garbage carried forward.
  for (int i = 7; i < DAILY_HISTORY_SLOTS; i++) {
    EXPECT_EQ(out.dailyHistory[i].date, 0) << "slot " << i;
    EXPECT_EQ(out.dailyHistory[i].minutes, 0) << "slot " << i;
  }

  // The next write must land at slot 7, not slot 0 — otherwise the very next day rollover
  // would immediately overwrite one of the 5 real migrated days a full cycle early.
  EXPECT_EQ(out.dailyHistoryWriteIndex, 7);
}

TEST(StatsMigration, SecondLoadOfMigratedFileTakesFastPath) {
  // First conversion: legacy in, current-format out, needsSave true (as StatsManager::save()
  // would honor by writing the versioned blob back to disk).
  auto legacyBuf = legacyFixtureBytes();
  GlobalStats migrated{};
  bool firstNeedsSave = false;
  ASSERT_TRUE(migrateStatsBlob(legacyBuf.data(), legacyBuf.size(), migrated, firstNeedsSave));
  ASSERT_TRUE(firstNeedsSave);

  // Simulate StatsManager::save() writing the migrated struct back with a leading version
  // byte, then StatsManager::load() reading that file back in on a later boot.
  std::array<uint8_t, sizeof(uint8_t) + sizeof(GlobalStats)> versionedBuf{};
  versionedBuf[0] = STATS_FILE_VERSION;
  memcpy(versionedBuf.data() + 1, &migrated, sizeof(GlobalStats));

  GlobalStats reloaded{};
  bool secondNeedsSave = true;  // Deliberately pre-set true so a bug that leaves it
                                 // untouched doesn't accidentally read as a pass.
  ASSERT_TRUE(migrateStatsBlob(versionedBuf.data(), versionedBuf.size(), reloaded, secondNeedsSave));

  // The migration writes back exactly once: a second load of the already-migrated,
  // already-versioned file must take the fast path, not re-run the legacy migration.
  EXPECT_FALSE(secondNeedsSave);
  EXPECT_EQ(memcmp(&reloaded, &migrated, sizeof(GlobalStats)), 0);
}

// Proves what the migration prevents: replicates the OLD naive fixed-size read
// (`file.read(reinterpret_cast<uint8_t*>(&stats), sizeof(GlobalStats))` into a
// zero-initialized, larger current-format struct) against the exact same legacy fixture,
// and shows it corrupts data instead of migrating it cleanly.
TEST(StatsMigration, NaiveShortReadCorruptsData) {
  auto legacyBuf = legacyFixtureBytes();

  // GlobalStats{} default-constructs every field to zero, exactly as the real
  // `StatsManager::stats` member does before load() ever touches it.
  GlobalStats naive{};
  // The old code path: blit only as many bytes as the (short) file actually contains,
  // straight into the raw memory of the larger current-format struct.
  memcpy(&naive, legacyBuf.data(), legacyBuf.size());

  // Legacy byte offset 84 (dailyHistoryWriteIndex, value 5 per the fixture) lands inside
  // the *new* struct's dailyHistory[7].date field (an int starting at the same byte
  // offset 84, since slots 0..6 occupy bytes 28..83 either way) — a stray, meaningless
  // date value that getLast7DaysMinutes() would render as real reading data.
  EXPECT_EQ(naive.dailyHistory[7].date, 5);

  // The new struct's real dailyHistoryWriteIndex (offset 700, well past the legacy file's
  // 88 bytes) is never touched by the short read, so it silently stays at its
  // default-constructed 0 instead of 7 — the next day rollover would overwrite slot 0
  // (one of the 5 real migrated days) a full cycle early.
  EXPECT_EQ(naive.dailyHistoryWriteIndex, 0);

  // Contrast: the real migration gets both of these right.
  GlobalStats migrated{};
  bool needsSave = false;
  ASSERT_TRUE(migrateStatsBlob(legacyBuf.data(), legacyBuf.size(), migrated, needsSave));
  EXPECT_EQ(migrated.dailyHistory[7].date, 0);
  EXPECT_EQ(migrated.dailyHistoryWriteIndex, 7);
}
