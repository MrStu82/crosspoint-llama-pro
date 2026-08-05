#include "BookProgressBadge.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>

#include "activities/reader/ProgressFile.h"

namespace {
constexpr char BADGE_FILENAME[] = "book_progress.bin";
}

namespace BookProgressBadge {

void write(const std::string& cachePath, int percent) {
  const uint8_t percentByte = static_cast<uint8_t>(std::clamp(percent, 0, 100));
  ProgressFile::writeAtomic(cachePath, &percentByte, 1, BADGE_FILENAME);
}

std::optional<int> read(const std::string& path) {
  std::string cachePath;
  if (FsHelpers::hasEpubExtension(path)) {
    cachePath = Epub(path, "/.crosspoint").getCachePath();
  } else if (FsHelpers::hasXtcExtension(path)) {
    cachePath = Xtc(path, "/.crosspoint").getCachePath();
  } else if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path)) {
    cachePath = Txt(path, "/.crosspoint").getCachePath();
  } else {
    return std::nullopt;
  }

  HalFile f;
  if (!Storage.openFileForRead("BADGE", cachePath + "/" + BADGE_FILENAME, f)) {
    return std::nullopt;
  }
  uint8_t percentByte = 0;
  const bool ok = f.read(&percentByte, 1) == 1;
  f.close();
  if (!ok) {
    return std::nullopt;
  }
  return static_cast<int>(percentByte);
}

}  // namespace BookProgressBadge
