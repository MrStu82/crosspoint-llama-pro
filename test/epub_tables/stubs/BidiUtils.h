#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace BidiUtils {
enum class BidiBaseDir : uint8_t { AUTO, LTR, RTL };
constexpr int RTL_PARAGRAPH_PROBE_DEPTH = 3;
inline bool startsWithRtl(const char*, int) { return false; }
inline uint8_t detectParagraphLevel(const char*, uint8_t fallback) { return fallback; }
inline bool computeVisualWordOrder(const std::vector<std::string>&, bool, std::vector<uint16_t>&) { return false; }
}  // namespace BidiUtils
