#include "ChapterProgress.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>

namespace ChapterProgress {

ChapterProgressValue read(const std::string& bookPath) {
  ChapterProgressValue result;
  // Only the EPUB reader persists a per-chapter page count; the other formats
  // have no spine to bound a chapter with.
  if (!FsHelpers::hasEpubExtension(bookPath)) return result;

  const std::string filename = Epub(bookPath, "/.crosspoint").getCachePath() + "/progress.bin";
  HalFile file;
  if (!Storage.openFileForRead("CHP", filename, file)) return result;

  // Layout written by EpubReaderUtils::saveProgress: spineIndex, pageNumber,
  // pageCount, then an optional visible-text offset. The 4-byte form predates
  // the page count, so it cannot answer this question.
  uint8_t data[6] = {};
  if (file.read(data, sizeof(data)) < static_cast<int>(sizeof(data))) return result;

  const int currentPage = data[2] | (data[3] << 8);
  const int pageCount = data[4] | (data[5] << 8);
  if (pageCount <= 0) return result;
  // UINT16_MAX is the reader's "open on last page" navigation sentinel, not a
  // real page index.
  if (currentPage == 0xFFFF) return result;

  result.currentPage = currentPage;
  result.pageCount = pageCount;
  result.available = true;
  return result;
}

}  // namespace ChapterProgress
