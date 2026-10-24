// Finished-book home rows on the real stores: BookReadingStats, ChapterProgress,
// BookProgressBadge and InkPointHomeGeometry, driven the way EpubReaderActivity
// drives them when the reader pages onto the End-of-Book screen and leaves.
#include <cassert>
#include <cstdio>
#include <string>

#include <Epub.h>
#include <HalStorage.h>

#include "activities/home/HomeInkPointGeometry.h"
#include "util/BookProgressBadge.h"
#include "util/BookReadingStats.h"
#include "util/ChapterProgress.h"

using namespace BookReadingRate;

static void fmt(char* out, size_t len, uint32_t minutes) {  // HomeActivity formatMinutes
  if (minutes == 0) std::snprintf(out, len, "<1m");
  else if (minutes < 60) std::snprintf(out, len, "%lum", static_cast<unsigned long>(minutes));
  else std::snprintf(out, len, "%luh %02lum", static_cast<unsigned long>(minutes / 60), static_cast<unsigned long>(minutes % 60));
}

struct Rows { std::string chapter, book; };

static Rows homeRows(const std::string& path) {
  const auto stats = BookReadingStats::read(path);
  const auto chapter = ChapterProgress::read(path);
  const int pct = BookProgressBadge::read(path).value_or(-1);
  Rows r{"CALIBRATING", "CALIBRATING"};
  if (!stats.available) return r;
  const auto eta = InkPointHomeGeometry::estimateEtas(stats.pagesPerMinuteQ16, stats.remainingPagesQ16,
                                                      stats.remainingAvailable, chapter.pagesLeft(), chapter.available);
#ifdef FINISHED_ROWS
  const auto rows = InkPointHomeGeometry::statRows(stats.finished || pct >= 100, eta);
  char buf[16];
  if (rows.chapter == InkPointHomeGeometry::StatValue::Finished) r.chapter = "Finished";
  else if (rows.chapter == InkPointHomeGeometry::StatValue::Minutes) { fmt(buf, sizeof buf, rows.chapterMinutes); r.chapter = buf; }
  if (rows.book == InkPointHomeGeometry::StatValue::Finished) r.book = "Finished";
  else if (rows.book == InkPointHomeGeometry::StatValue::Minutes) { fmt(buf, sizeof buf, rows.bookMinutes); r.book = buf; }
#else
  char buf[16];
  if (eta.chapterMinutes) { fmt(buf, sizeof buf, *eta.chapterMinutes); r.chapter = buf; }
  if (eta.bookMinutes) { fmt(buf, sizeof buf, *eta.bookMinutes); r.book = buf; }
#endif
  std::printf("  stats: remainingAvailable=%d remainingPagesQ16=%u ppmQ16=%u | chapter page=%d/%d | badge=%d\n",
              stats.remainingAvailable, stats.remainingPagesQ16, stats.pagesPerMinuteQ16, chapter.currentPage,
              chapter.pageCount, pct);
  return r;
}

static void writeChapter(const std::string& path, uint16_t spine, uint16_t page, uint16_t count) {
  const uint8_t b[6] = {uint8_t(spine), uint8_t(spine >> 8), uint8_t(page), uint8_t(page >> 8), uint8_t(count), uint8_t(count >> 8)};
  Storage.files[Epub(path, "/.crosspoint").getCachePath() + "/progress.bin"] = {b, b + 6};
}

static void moveCache(const std::string& from, const std::string& to) {  // directory rename on the SD card
  const std::string a = Epub(from, "").getCachePath(), b = Epub(to, "").getCachePath();
  for (auto it = Storage.files.begin(); it != Storage.files.end();) {
    if (it->first.rfind(a, 0) == 0) { Storage.files[b + it->first.substr(a.size())] = it->second; it = Storage.files.erase(it); }
    else ++it;
  }
}

int main() {
  const std::string path = "/Books/book.epub", moved = "/Read/book.epub";
  const uint32_t fp = 123, hash = hashString(path.c_str());
  // Read the last chapter: qualified 30 s dwells, 1% of the book per page.
  uint32_t progress = kQ24One - 11 * (kQ24One / 100);
  for (int i = 0; i < 10; ++i) {
    const uint32_t before = progress;
    progress += kQ24One / 100;
    assert(BookReadingStats::recordQualifiedPage(path, {30, fp, hash, ContentBasis::FineProgress, 0, progress, progress - before}, 30));
  }
  writeChapter(path, 7, 9, 10);  // last saveProgress: final page of the last chapter
  std::printf("last page, before End-of-Book screen:\n");
  Rows r = homeRows(path);
  std::printf("  CHAPTER LEFT=%s BOOK LEFT=%s\n", r.chapter.c_str(), r.book.c_str());

  // End-of-Book screen: badge 100, no saveProgress; onExit stores kQ24One.
  BookProgressBadge::write(Epub(path, "/.crosspoint").getCachePath(), 100);
  assert(BookReadingStats::updatePosition(path, fp, ContentBasis::FineProgress, 0, kQ24One));
  assert(BookReadingStats::add(path, 40, 0));
  std::printf("after End-of-Book screen + exit (no /Read/ move):\n");
  r = homeRows(path);
  std::printf("  CHAPTER LEFT=%s BOOK LEFT=%s\n", r.chapter.c_str(), r.book.c_str());
  const Rows inPlace = r;

  moveCache(path, moved);
  std::printf("after /Read/ move (home reads the new path):\n");
  r = homeRows(moved);
  std::printf("  CHAPTER LEFT=%s BOOK LEFT=%s\n", r.chapter.c_str(), r.book.c_str());

#ifdef FINISHED_ROWS
  assert(inPlace.chapter == "Finished" && inPlace.book == "Finished");
  assert(r.chapter == "Finished" && r.book == "Finished");
  std::printf("PASS finished rows\n");
#else
  std::printf("BASE (no finished state): in-place %s/%s, moved %s/%s\n", inPlace.chapter.c_str(), inPlace.book.c_str(),
              r.chapter.c_str(), r.book.c_str());
#endif
  return 0;
}
