#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>

namespace BookReadingRate {

inline constexpr uint32_t kQ16One = 1U << 16;
inline constexpr uint32_t kQ24One = 1U << 24;
inline constexpr uint32_t kMinDwellMs = 5000;
inline constexpr uint32_t kMaxDwellMs = 300000;
inline constexpr uint8_t kBookSampleCapacity = 15;
inline constexpr uint8_t kOverallSampleCapacity = 31;
inline constexpr uint8_t kCurrentMinSamples = 5;
inline constexpr uint32_t kCurrentMinSeconds = 300;
inline constexpr uint8_t kOverallMinSamples = 20;
inline constexpr uint8_t kOverallMinBooks = 2;
inline constexpr uint32_t kBookMagic = 0x32535242U;     // BRS2
inline constexpr uint32_t kOverallMagic = 0x32525242U;  // BRR2
inline constexpr uint8_t kVersion = 2;

enum class ContentBasis : uint8_t { Unknown = 0, ExactPages = 1, FineProgress = 2 };

struct RateSample {
  uint16_t dwellSeconds = 0;
  uint16_t reserved = 0;
  uint32_t progressDeltaQ24 = 0;
};

struct OverallSample {
  uint16_t dwellSeconds = 0;
  uint16_t reserved = 0;
  uint32_t fingerprint = 0;
  uint32_t bookHash = 0;
};

#pragma pack(push, 1)
struct LegacyBookV1 {
  uint8_t version = 1;
  uint8_t reserved[3]{};
  uint32_t totalSeconds = 0;
  uint32_t forwardPages = 0;
};

struct StoredBookV2 {
  uint32_t magic = kBookMagic;
  uint8_t version = kVersion;
  uint8_t sampleCount = 0;
  uint8_t sampleNext = 0;
  ContentBasis basis = ContentBasis::Unknown;
  uint32_t totalSeconds = 0;
  uint32_t fingerprint = 0;
  uint32_t remainingPagesQ16 = 0;
  uint32_t progressQ24 = 0;
  std::array<RateSample, kBookSampleCapacity> samples{};
  uint32_t checksum = 0;
};

struct StoredOverallV2 {
  uint32_t magic = kOverallMagic;
  uint8_t version = kVersion;
  uint8_t sampleCount = 0;
  uint8_t sampleNext = 0;
  uint8_t reserved = 0;
  std::array<OverallSample, kOverallSampleCapacity> samples{};
  uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(sizeof(LegacyBookV1) == 12);

inline uint32_t fnv1a(const uint8_t* data, size_t len, uint32_t hash = 2166136261U) {
  for (size_t i = 0; i < len; ++i) {
    hash ^= data[i];
    hash *= 16777619U;
  }
  return hash;
}

inline uint32_t hashValue(uint32_t hash, uint32_t value) {
  return fnv1a(reinterpret_cast<const uint8_t*>(&value), sizeof(value), hash);
}

inline uint32_t hashString(const char* text) {
  if (!text) return 2166136261U;
  return fnv1a(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
}

inline uint32_t layoutFingerprint(uint32_t readerKind, uint32_t screenWidth, uint32_t screenHeight,
                                  uint32_t fontId, uint32_t fontPointSize, uint32_t lineSpacing,
                                  uint32_t margin, uint32_t alignment, uint32_t orientation,
                                  uint32_t paginationFlags) {
  uint32_t hash = 2166136261U;
  for (const uint32_t value : {readerKind, screenWidth, screenHeight, fontId, fontPointSize, lineSpacing,
                               margin, alignment, orientation, paginationFlags})
    hash = hashValue(hash, value);
  return hash == 0 ? 1 : hash;
}

inline uint32_t pageKey(uint32_t fingerprint, uint32_t major, uint32_t minor = 0) {
  return hashValue(hashValue(fingerprint, major), minor);
}

inline uint32_t progressQ24(float progress) {
  if (!(progress > 0.0F)) return 0;
  if (progress >= 1.0F) return kQ24One;
  return static_cast<uint32_t>(progress * static_cast<float>(kQ24One) + 0.5F);
}

inline uint32_t exactRemainingQ16(uint32_t currentPage, uint32_t totalPages) {
  if (totalPages == 0 || currentPage >= totalPages - 1) return 0;
  const uint64_t remaining = static_cast<uint64_t>(totalPages - currentPage - 1) * kQ16One;
  return static_cast<uint32_t>(std::min<uint64_t>(remaining, std::numeric_limits<uint32_t>::max()));
}

inline uint32_t median(std::array<uint32_t, kOverallSampleCapacity>& values, size_t count) {
  if (count == 0) return 0;
  std::sort(values.begin(), values.begin() + static_cast<ptrdiff_t>(count));
  if (count & 1U) return values[count / 2];
  return static_cast<uint32_t>((static_cast<uint64_t>(values[count / 2 - 1]) + values[count / 2]) / 2U);
}

inline uint32_t pagesPerMinuteQ16(const uint16_t* dwellSeconds, size_t count) {
  if (!dwellSeconds || count == 0 || count > kOverallSampleCapacity) return 0;
  std::array<uint32_t, kOverallSampleCapacity> rates{};
  size_t used = 0;
  for (size_t i = 0; i < count; ++i) {
    if (dwellSeconds[i] == 0) continue;
    rates[used++] = static_cast<uint32_t>((60ULL * kQ16One) / dwellSeconds[i]);
  }
  return median(rates, used);
}

inline bool currentConfident(size_t sampleCount, uint32_t activeSeconds) {
  return sampleCount >= kCurrentMinSamples && activeSeconds >= kCurrentMinSeconds;
}

inline bool overallConfident(size_t sampleCount, size_t distinctBooks) {
  return sampleCount >= kOverallMinSamples && distinctBooks >= kOverallMinBooks;
}

enum class RateSource : uint8_t { None, CurrentBook, OverallFallback };
struct SelectedRate {
  uint32_t pagesPerMinuteQ16 = 0;
  RateSource source = RateSource::None;
};
inline SelectedRate selectRate(uint32_t currentRateQ16, bool currentIsConfident,
                               uint32_t overallRateQ16, bool overallIsConfident) {
  if (currentIsConfident && currentRateQ16 != 0) return {currentRateQ16, RateSource::CurrentBook};
  if (overallIsConfident && overallRateQ16 != 0) return {overallRateQ16, RateSource::OverallFallback};
  return {};
}

inline uint32_t remainingFromFineProgressQ16(uint32_t progress, const uint32_t* deltas, size_t count) {
  if (progress >= kQ24One || !deltas || count == 0 || count > kBookSampleCapacity) return 0;
  std::array<uint32_t, kOverallSampleCapacity> valid{};
  size_t used = 0;
  for (size_t i = 0; i < count; ++i)
    if (deltas[i] > 0) valid[used++] = deltas[i];
  const uint32_t perPage = median(valid, used);
  if (perPage == 0) return 0;
  const uint64_t q16 = (static_cast<uint64_t>(kQ24One - progress) << 16) / perPage;
  return static_cast<uint32_t>(std::min<uint64_t>(q16, std::numeric_limits<uint32_t>::max()));
}

inline std::optional<uint32_t> etaMinutes(uint32_t remainingPagesQ16, uint32_t pagesPerMinuteQ16Value) {
  if (pagesPerMinuteQ16Value == 0) return std::nullopt;
  const uint64_t minutes = static_cast<uint64_t>(remainingPagesQ16) / pagesPerMinuteQ16Value;
  return static_cast<uint32_t>(std::min<uint64_t>(minutes, std::numeric_limits<uint32_t>::max()));
}

inline uint32_t checksumBytes(const void* data, size_t len) {
  return fnv1a(static_cast<const uint8_t*>(data), len);
}

inline void seal(StoredBookV2& stored) {
  stored.checksum = 0;
  stored.checksum = checksumBytes(&stored, sizeof(stored));
}
inline void seal(StoredOverallV2& stored) {
  stored.checksum = 0;
  stored.checksum = checksumBytes(&stored, sizeof(stored));
}
inline bool valid(const StoredBookV2& stored) {
  if (stored.magic != kBookMagic || stored.version != kVersion || stored.sampleCount > kBookSampleCapacity ||
      stored.sampleNext >= kBookSampleCapacity)
    return false;
  StoredBookV2 copy = stored;
  const uint32_t expected = copy.checksum;
  seal(copy);
  return expected == copy.checksum;
}
inline bool valid(const StoredOverallV2& stored) {
  if (stored.magic != kOverallMagic || stored.version != kVersion || stored.sampleCount > kOverallSampleCapacity ||
      stored.sampleNext >= kOverallSampleCapacity)
    return false;
  StoredOverallV2 copy = stored;
  const uint32_t expected = copy.checksum;
  seal(copy);
  return expected == copy.checksum;
}

inline StoredBookV2 migrate(const LegacyBookV1& legacy) {
  StoredBookV2 migrated;
  if (legacy.version == 1) migrated.totalSeconds = legacy.totalSeconds;
  // Legacy forwardPages is deliberately discarded: it has no qualified dwell evidence.
  seal(migrated);
  return migrated;
}

class DwellTracker {
  std::atomic<uint32_t> visibleAtMs_{0};
  std::atomic<uint32_t> pageKey_{0};
  std::atomic<bool> active_{false};

 public:
  void markVisible(uint32_t nowMs, uint32_t key) {
    if (active_.load(std::memory_order_acquire) && pageKey_.load(std::memory_order_relaxed) == key) return;
    pageKey_.store(key, std::memory_order_relaxed);
    visibleAtMs_.store(nowMs, std::memory_order_relaxed);
    active_.store(true, std::memory_order_release);
  }

  void pause() { active_.store(false, std::memory_order_release); }

  std::optional<uint16_t> takeQualifiedForward(uint32_t nowMs, uint32_t expectedKey, bool singlePage) {
    if (!singlePage || !active_.exchange(false, std::memory_order_acq_rel) ||
        pageKey_.load(std::memory_order_relaxed) != expectedKey)
      return std::nullopt;
    const uint32_t elapsed = nowMs - visibleAtMs_.load(std::memory_order_relaxed);  // wrap-safe
    if (elapsed < kMinDwellMs || elapsed > kMaxDwellMs) return std::nullopt;
    return static_cast<uint16_t>(elapsed / 1000U);
  }
};

}  // namespace BookReadingRate
