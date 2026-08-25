#include "BookReadingStats.h"

#include <algorithm>
#include <limits>

#include <Epub.h>
#include <FsHelpers.h>
#include <Txt.h>
#include <Xtc.h>
#include "activities/reader/ProgressFile.h"

namespace {
constexpr uint8_t kVersion = 1;
struct Stored {
  uint8_t version;
  uint8_t reserved[3];
  uint32_t totalSeconds;
  uint32_t forwardPages;
};
static_assert(sizeof(Stored) == 12);

std::string cachePath(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) return Epub(path, "/.crosspoint").getCachePath();
  if (FsHelpers::hasXtcExtension(path)) return Xtc(path, "/.crosspoint").getCachePath();
  if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path))
    return Txt(path, "/.crosspoint").getCachePath();
  return "/.crosspoint/home";
}
}  // namespace

namespace BookReadingStats {
BookReadingStatsValue read(const std::string& bookPath) {
  BookReadingStatsValue result;
  HalFile file;
  const std::string filename = cachePath(bookPath) + "/reading_stats.bin";
  if (!Storage.openFileForRead("BRS", filename, file) || file.size() != sizeof(Stored)) return result;
  Stored stored{};
  if (file.read(reinterpret_cast<uint8_t*>(&stored), sizeof(stored)) != sizeof(stored) || stored.version != kVersion)
    return result;
  result.totalSeconds = stored.totalSeconds;
  result.forwardPages = stored.forwardPages;
  return result;
}

bool add(const std::string& bookPath, uint32_t seconds, uint32_t forwardPages) {
  if (seconds == 0 && forwardPages == 0) return true;
  auto current = read(bookPath);
  const auto satAdd = [](uint32_t a, uint32_t b) {
    return b > std::numeric_limits<uint32_t>::max() - a ? std::numeric_limits<uint32_t>::max() : a + b;
  };
  Stored stored{kVersion, {0, 0, 0}, satAdd(current.totalSeconds, seconds), satAdd(current.forwardPages, forwardPages)};
  const std::string dir = cachePath(bookPath);
  if (!Storage.exists(dir.c_str()) && !Storage.mkdir(dir.c_str())) return false;
  return ProgressFile::writeAtomic(dir, reinterpret_cast<const uint8_t*>(&stored), sizeof(stored), "reading_stats.bin");
}
}  // namespace BookReadingStats
