// #x4pro-gzflash: .bin.gz SD firmware path. Fixtures come from the runner:
//   GZFW_APP        real release app.bin (ESP32-S3 image)
//   GZFW_APP_GZ     `gzip -9n` of GZFW_APP
//   GZFW_BADIMG_GZ  `gzip -9n` of GZFW_APP with one image byte flipped (valid gzip, invalid image)
#include <BuildScratch.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "FirmwareFlasher.h"
#include "HalStorage.h"
#include "OtaBootSwitch.h"
#include "esp_ota_ops.h"

HalStorage Storage;
const esp_partition_t* g_nextUpdate = nullptr;
const esp_partition_t* g_running = nullptr;

namespace buildscratch {
uint8_t* claim(size_t, size_t*) { return nullptr; }
void release(const uint8_t*) {}
}  // namespace buildscratch

static int g_switches = 0;
namespace ota_boot {
bool switchTo(const esp_partition_t*) {
  ++g_switches;
  return true;
}
}  // namespace ota_boot

namespace {
constexpr uint32_t PART_SIZE = 0x640000;

std::string load(const char* env) {
  const char* path = std::getenv(env);
  if (!path) return {};
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}

struct Fixture : ::testing::Test {
  std::string app = load("GZFW_APP");
  std::string gz = load("GZFW_APP_GZ");
  std::vector<uint8_t> destBytes = std::vector<uint8_t>(PART_SIZE, 0x00);  // unerased: writes must erase first
  std::vector<uint8_t> runBytes;
  esp_partition_t dest{"app1", 0x650000, PART_SIZE, &destBytes};
  esp_partition_t running{"app0", 0x10000, PART_SIZE, &runBytes};
  size_t lastWritten = 0, lastTotal = 0;

  void SetUp() override {
    ASSERT_GT(app.size(), 64u * 1024) << "GZFW_APP missing";
    ASSERT_GT(gz.size(), 18u) << "GZFW_APP_GZ missing";
    ASSERT_LT(gz.size(), app.size());
    runBytes.assign(app.begin(), app.begin() + 24);  // running image header: same chip id
    g_nextUpdate = &dest;
    g_running = &running;
    g_switches = 0;
    Storage.files.clear();
  }

  firmware_flash::Result flashGz(const std::string& bytes) {
    Storage.files["/fw.bin.gz"] = bytes;
    return firmware_flash::flashGzFromSdPath("/fw.bin.gz", &Fixture::progress, this);
  }
  static void progress(size_t written, size_t total, void* ctx) {
    auto* self = static_cast<Fixture*>(ctx);
    self->lastWritten = written;
    self->lastTotal = total;
  }
  bool destHoldsApp() const { return std::memcmp(app.data(), destBytes.data(), app.size()) == 0; }
};
}  // namespace

TEST_F(Fixture, GzRoundTripIsByteIdenticalAndSwitchesOnce) {
  ASSERT_EQ(flashGz(gz), firmware_flash::Result::OK);
  EXPECT_TRUE(destHoldsApp());
  EXPECT_EQ(g_switches, 1);
  EXPECT_EQ(lastWritten, gz.size() - 8);  // last report precedes the 8-byte CRC32/ISIZE trailer read
  EXPECT_EQ(lastTotal, gz.size());
}

TEST_F(Fixture, CorruptGzIsRejectedWithoutBootSwitch) {
  // Deflate body (start, middle, near end) and the CRC32/ISIZE trailer.
  for (const size_t off : {size_t{20}, gz.size() / 2, gz.size() - 12, gz.size() - 6, gz.size() - 2}) {
    std::string bad = gz;
    bad[off] = static_cast<char>(bad[off] ^ 0x5A);
    EXPECT_NE(flashGz(bad), firmware_flash::Result::OK) << "offset " << off;
    EXPECT_EQ(g_switches, 0) << "offset " << off;
  }
  std::string notGz = gz;
  notGz[0] = 0;
  EXPECT_EQ(flashGz(notGz), firmware_flash::Result::BAD_GZIP);
  EXPECT_EQ(g_switches, 0);
}

TEST_F(Fixture, TruncatedGzIsRejectedWithoutBootSwitch) {
  for (const size_t len : {size_t{10}, gz.size() / 2, gz.size() - 9, gz.size() - 8, gz.size() - 1}) {
    EXPECT_NE(flashGz(gz.substr(0, len)), firmware_flash::Result::OK) << "len " << len;
    EXPECT_EQ(g_switches, 0) << "len " << len;
  }
}

TEST_F(Fixture, ValidGzipOfInvalidImageIsRejectedAfterInflate) {
  const std::string badImg = load("GZFW_BADIMG_GZ");
  ASSERT_GT(badImg.size(), 18u) << "GZFW_BADIMG_GZ missing";
  const auto r = flashGz(badImg);
  EXPECT_NE(r, firmware_flash::Result::OK);
  EXPECT_NE(r, firmware_flash::Result::BAD_GZIP) << "gzip layer must pass; the image check must catch it";
  EXPECT_EQ(g_switches, 0);
}

TEST_F(Fixture, GzLargerThanPartitionIsRejected) {
  dest.size = 1024 * 1024;
  EXPECT_EQ(flashGz(gz), firmware_flash::Result::BAD_GZIP);
  EXPECT_EQ(g_switches, 0);
}

TEST_F(Fixture, PlainBinPathUnchanged) {
  Storage.files["/fw.bin"] = app;
  EXPECT_EQ(firmware_flash::validateImageFile("/fw.bin", PART_SIZE), firmware_flash::Result::OK);
  ASSERT_EQ(firmware_flash::flashFromSdPath("/fw.bin", &Fixture::progress, this), firmware_flash::Result::OK);
  EXPECT_TRUE(destHoldsApp());
  EXPECT_EQ(g_switches, 1);
  EXPECT_EQ(lastWritten, app.size());
}
