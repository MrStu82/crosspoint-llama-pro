#pragma once

namespace reader_diagnostics {
enum class Stage { Persistence, TxtOpen, TxtLayout, EpubLayout, RenderLock, Refresh, Count };
}

// Opt-in compiler flag only; production has no timers, buffers, logging or UI.
#if defined(X4_READER_DIAGNOSTICS) && X4_READER_DIAGNOSTICS
#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <Logging.h>
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <chrono>
#endif
namespace reader_diagnostics {
constexpr size_t Samples = 200;
struct Metrics {
  std::array<uint64_t, Samples> us{};
  size_t count = 0;
  uint32_t heapMin = UINT32_MAX, largestMin = UINT32_MAX, stackWordsMin = UINT32_MAX;
};
inline Metrics metrics[static_cast<size_t>(Stage::Count)];
inline std::mutex metricsMutex;
inline uint64_t now() {
#if defined(ESP_PLATFORM)
  return esp_timer_get_time();
#else
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
class Scope {
  Stage stage; uint64_t start;
 public:
  explicit Scope(Stage s) : stage(s), start(now()) {}
  ~Scope() {
    const uint64_t elapsed = now() - start;
    std::lock_guard<std::mutex> lock(metricsMutex);
    auto& m = metrics[static_cast<size_t>(stage)];
    if (m.count == Samples) return;
    m.us[m.count++] = elapsed;
#if defined(ESP_PLATFORM)
    m.heapMin = std::min(m.heapMin, static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    m.largestMin = std::min(m.largestMin, static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
    m.stackWordsMin = std::min(m.stackWordsMin, static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)));
#endif
    if (m.count == 1 || m.count == Samples) {
      auto ordered = m.us;
      std::sort(ordered.begin(), ordered.begin() + m.count);
      LOG_INF("RPROF", "stage=%u n=%u p50_us=%llu p95_us=%llu max_us=%llu heap_min=%u largest_min=%u stack_words_min=%u",
          static_cast<unsigned>(stage), static_cast<unsigned>(m.count),
          static_cast<unsigned long long>(ordered[(m.count - 1) / 2]),
          static_cast<unsigned long long>(ordered[(m.count * 95 + 99) / 100 - 1]),
          static_cast<unsigned long long>(ordered[m.count - 1]), m.heapMin, m.largestMin, m.stackWordsMin);
    }
  }
};
}
#else
namespace reader_diagnostics {
class Scope { public: explicit Scope(Stage) {} };
}
#endif
