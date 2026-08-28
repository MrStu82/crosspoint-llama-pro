#pragma once

#include <cstddef>
#include <cstdint>

namespace DictHtmlPageBudget {

constexpr size_t MAX_PAGES = 64;
constexpr size_t MAX_ELEMENTS = 512;
constexpr uint32_t MIN_RETAIN_FREE_HEAP = 16 * 1024;
constexpr uint32_t MIN_RETAIN_MAX_ALLOC = 8 * 1024;

enum class Limit : uint8_t { None, PageCount, ElementCount, Heap };

constexpr Limit retainedPageLimit(size_t pageCount, size_t retainedElements, size_t nextPageElements,
                                  uint32_t freeHeap, uint32_t maxAllocHeap) {
  if (pageCount >= MAX_PAGES) return Limit::PageCount;
  if (nextPageElements > MAX_ELEMENTS - retainedElements) return Limit::ElementCount;
  if (freeHeap < MIN_RETAIN_FREE_HEAP || maxAllocHeap < MIN_RETAIN_MAX_ALLOC) return Limit::Heap;
  return Limit::None;
}

constexpr const char* limitName(Limit limit) {
  switch (limit) {
    case Limit::PageCount:
      return "page count";
    case Limit::ElementCount:
      return "element count";
    case Limit::Heap:
      return "free heap";
    case Limit::None:
    default:
      return nullptr;
  }
}

}  // namespace DictHtmlPageBudget
