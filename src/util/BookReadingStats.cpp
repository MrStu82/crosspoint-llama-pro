#include "BookReadingStats.h"

#include <algorithm>
#include <array>
#include <limits>

#include <Epub.h>
#include <FsHelpers.h>
#include <Txt.h>
#include <Xtc.h>
#include "activities/reader/ProgressFile.h"

namespace {
using namespace BookReadingRate;
constexpr char BOOK_FILENAME[] = "reading_stats.bin";
constexpr char OVERALL_DIR[] = "/.crosspoint";
constexpr char OVERALL_FILENAME[] = "reading_rate_v2.bin";

std::string cachePath(const std::string& path) {
  if (FsHelpers::hasEpubExtension(path)) return Epub(path, "/.crosspoint").getCachePath();
  if (FsHelpers::hasXtcExtension(path)) return Xtc(path, "/.crosspoint").getCachePath();
  if (FsHelpers::hasTxtExtension(path) || FsHelpers::hasMarkdownExtension(path))
    return Txt(path, "/.crosspoint").getCachePath();
  return "/.crosspoint/home";
}

uint32_t satAdd(uint32_t a, uint32_t b) {
  return b > std::numeric_limits<uint32_t>::max() - a ? std::numeric_limits<uint32_t>::max() : a + b;
}

bool ensureDir(const std::string& dir) { return Storage.exists(dir.c_str()) || Storage.mkdir(dir.c_str()); }

bool loadBook(const std::string& path, StoredBookV3& stored, bool& available, bool& needsSave) {
  available = false;
  needsSave = false;
  HalFile file;
  const std::string filename = cachePath(path) + "/" + BOOK_FILENAME;
  if (!Storage.openFileForRead("BRS", filename, file)) return true;
  if (file.size() == sizeof(LegacyBookV1)) {
    LegacyBookV1 legacy{};
    if (file.read(reinterpret_cast<uint8_t*>(&legacy), sizeof(legacy)) != sizeof(legacy) || legacy.version != 1)
      return false;
    stored = migrate(legacy);
    available = true;
    needsSave = true;
    return true;
  }
  if (file.size() == sizeof(StoredBookV2)) {
    StoredBookV2 old{};
    if (file.read(reinterpret_cast<uint8_t*>(&old), sizeof(old)) != sizeof(old) || !valid(old)) return false;
    stored = migrate(old);
    available = true;
    needsSave = true;
    return true;
  }
  if (file.size() != sizeof(StoredBookV3) ||
      file.read(reinterpret_cast<uint8_t*>(&stored), sizeof(stored)) != sizeof(stored) || !valid(stored)) return false;
  available = true;
  return true;
}

bool saveBook(const std::string& path, StoredBookV3& stored) {
  const std::string dir = cachePath(path);
  if (!ensureDir(dir)) return false;
  seal(stored);
  return ProgressFile::writeAtomic(dir, reinterpret_cast<const uint8_t*>(&stored), sizeof(stored), BOOK_FILENAME);
}

bool loadOverall(StoredOverallV2& stored) {
  HalFile file;
  const std::string filename = std::string(OVERALL_DIR) + "/" + OVERALL_FILENAME;
  if (!Storage.openFileForRead("BRR", filename, file)) return true;
  return file.size() == sizeof(stored) && file.read(reinterpret_cast<uint8_t*>(&stored), sizeof(stored)) == sizeof(stored) &&
         valid(stored);
}

bool saveOverall(StoredOverallV2& stored) {
  if (!ensureDir(OVERALL_DIR)) return false;
  seal(stored);
  return ProgressFile::writeAtomic(OVERALL_DIR, reinterpret_cast<const uint8_t*>(&stored), sizeof(stored),
                                   OVERALL_FILENAME);
}

template <typename T, size_t N>
void append(std::array<T, N>& values, uint8_t& count, uint8_t& next, const T& value) {
  values[next] = value;
  next = static_cast<uint8_t>((next + 1U) % N);
  if (count < N) ++count;
}

uint32_t currentRate(const StoredBookV3& stored, uint32_t& seconds, bool& confident) {
  std::array<uint16_t, kBookSampleCapacity> dwell{};
  seconds = 0;
  for (uint8_t i = 0; i < stored.sampleCount; ++i) {
    dwell[i] = stored.samples[i].dwellSeconds;
    seconds = satAdd(seconds, dwell[i]);
  }
  confident = currentConfident(stored.sampleCount, seconds);
  return pagesPerMinuteQ16(dwell.data(), stored.sampleCount);
}

uint32_t overallRate(const StoredOverallV2& stored, uint32_t fingerprint, bool& confident) {
  std::array<uint16_t, kOverallSampleCapacity> dwell{};
  std::array<uint32_t, kOverallSampleCapacity> books{};
  size_t count = 0;
  size_t bookCount = 0;
  for (uint8_t i = 0; i < stored.sampleCount; ++i) {
    const auto& sample = stored.samples[i];
    if (sample.fingerprint != fingerprint || sample.dwellSeconds == 0) continue;
    dwell[count++] = sample.dwellSeconds;
    if (std::find(books.begin(), books.begin() + static_cast<ptrdiff_t>(bookCount), sample.bookHash) ==
        books.begin() + static_cast<ptrdiff_t>(bookCount))
      books[bookCount++] = sample.bookHash;
  }
  confident = overallConfident(count, bookCount);
  return pagesPerMinuteQ16(dwell.data(), count);
}

uint32_t fineRemaining(const StoredBookV3& stored) {
  std::array<uint32_t, kBookSampleCapacity> deltas{};
  for (uint8_t i = 0; i < stored.sampleCount; ++i) deltas[i] = stored.samples[i].progressDeltaQ24;
  return remainingFromFineProgressQ16(stored.progressQ24, deltas.data(), stored.sampleCount);
}
}  // namespace

namespace BookReadingStats {
BookReadingStatsValue read(const std::string& bookPath) {
  BookReadingStatsValue result;
  StoredBookV3 stored;
  bool available = false;
  bool needsSave = false;
  if (!loadBook(bookPath, stored, available, needsSave) || !available) return result;
  // V1/V2 upgrades are persisted through the same atomic rename path as all
  // other reader stats. A write failure does not hide the valid in-memory rate.
  if (needsSave) saveBook(bookPath, stored);

  result.available = true;
  result.totalSeconds = stored.totalSeconds;
  result.fingerprint = stored.fingerprint;
  result.qualifiedSamples = stored.sampleCount;
  result.remainingPagesQ16 = stored.basis == ContentBasis::FineProgress ? fineRemaining(stored)
                                                                         : stored.remainingPagesQ16;
  result.remainingAvailable = stored.basis != ContentBasis::Unknown &&
                              (stored.basis != ContentBasis::FineProgress || result.remainingPagesQ16 != 0 ||
                               stored.progressQ24 == kQ24One);

  bool currentConfident = false;
  uint32_t current = currentRate(stored, result.qualifiedSeconds, currentConfident);
  if (current == 0 && stored.legacyPagesPerMinuteQ16 != 0) {
    current = stored.legacyPagesPerMinuteQ16;
    result.legacyRate = true;
  }
  uint32_t overallValue = 0;
  bool overallIsConfident = false;
  if (current == 0 && stored.fingerprint != 0) {
    StoredOverallV2 overall;
    if (loadOverall(overall)) overallValue = overallRate(overall, stored.fingerprint, overallIsConfident);
  }
  const auto selected = selectRate(current, currentConfident, overallValue, overallIsConfident);
  result.pagesPerMinuteQ16 = selected.pagesPerMinuteQ16;
  result.currentRate = selected.source == RateSource::CurrentBook;
  result.fallbackRate = selected.source == RateSource::OverallFallback;
  result.rateConfident = selected.confident;
  return result;
}

bool add(const std::string& bookPath, uint32_t seconds, uint32_t) {
  if (seconds == 0) return true;
  StoredBookV3 stored;
  bool available = false;
  bool needsSave = false;
  if (!loadBook(bookPath, stored, available, needsSave)) stored = StoredBookV3{};
  stored.totalSeconds = satAdd(stored.totalSeconds, seconds);
  return saveBook(bookPath, stored);
}

bool recordQualifiedPage(const std::string& bookPath, const QualifiedPageSample& sample) {
  if (sample.dwellSeconds < kMinDwellMs / 1000U || sample.dwellSeconds > kMaxDwellMs / 1000U ||
      sample.fingerprint == 0 || sample.bookHash == 0 || sample.basis == ContentBasis::Unknown)
    return false;

  StoredBookV3 stored;
  bool available = false;
  bool needsSave = false;
  if (!loadBook(bookPath, stored, available, needsSave)) stored = StoredBookV3{};
  if (stored.fingerprint != sample.fingerprint) {
    const uint32_t preservedSeconds = stored.totalSeconds;
    const uint32_t preservedLegacyRate = stored.fingerprint == 0 ? stored.legacyPagesPerMinuteQ16 : 0;
    stored = StoredBookV3{};
    stored.totalSeconds = preservedSeconds;
    stored.fingerprint = sample.fingerprint;
    stored.legacyPagesPerMinuteQ16 = preservedLegacyRate;
  }
  stored.basis = sample.basis;
  stored.progressQ24 = sample.progressQ24;
  if (sample.basis == ContentBasis::ExactPages) {
    const uint64_t q16 = static_cast<uint64_t>(sample.exactRemainingPages) * kQ16One;
    stored.remainingPagesQ16 = static_cast<uint32_t>(std::min<uint64_t>(q16, std::numeric_limits<uint32_t>::max()));
  }
  append(stored.samples, stored.sampleCount, stored.sampleNext,
         RateSample{sample.dwellSeconds, 0, sample.progressDeltaQ24});
  const bool bookSaved = saveBook(bookPath, stored);

  StoredOverallV2 overall;
  if (!loadOverall(overall)) overall = StoredOverallV2{};
  append(overall.samples, overall.sampleCount, overall.sampleNext,
         OverallSample{sample.dwellSeconds, 0, sample.fingerprint, sample.bookHash});
  const bool overallSaved = saveOverall(overall);
  return bookSaved && overallSaved;
}

bool updatePosition(const std::string& bookPath, const uint32_t fingerprint,
                    const ContentBasis basis, const uint32_t exactRemainingPages,
                    const uint32_t progressQ24) {
  if (fingerprint == 0 || basis == ContentBasis::Unknown) return false;
  StoredBookV3 stored;
  bool available = false;
  bool needsSave = false;
  if (!loadBook(bookPath, stored, available, needsSave)) stored = StoredBookV3{};
  if (stored.fingerprint != fingerprint) {
    const uint32_t preservedSeconds = stored.totalSeconds;
    const uint32_t preservedLegacyRate = stored.fingerprint == 0 ? stored.legacyPagesPerMinuteQ16 : 0;
    stored = StoredBookV3{};
    stored.totalSeconds = preservedSeconds;
    stored.fingerprint = fingerprint;
    stored.legacyPagesPerMinuteQ16 = preservedLegacyRate;
  }
  stored.basis = basis;
  stored.progressQ24 = progressQ24;
  if (basis == ContentBasis::ExactPages) {
    const uint64_t q16 = static_cast<uint64_t>(exactRemainingPages) * kQ16One;
    stored.remainingPagesQ16 = static_cast<uint32_t>(std::min<uint64_t>(q16, std::numeric_limits<uint32_t>::max()));
  }
  return saveBook(bookPath, stored);
}
}  // namespace BookReadingStats
