#pragma once

#include <Arduino.h>
#include <Epub/Page.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class Epub;
class GfxRenderer;

namespace DictionaryParserStub {
inline size_t pagesToEmit = 2;
inline size_t elementsPerPage = 4;
inline bool parseOk = true;
inline uint32_t callbackFreeHeap = 29400;
inline uint32_t callbackMaxAlloc = 12000;

inline void reset() {
  pagesToEmit = 2;
  elementsPerPage = 4;
  parseOk = true;
  callbackFreeHeap = 29400;
  callbackMaxAlloc = 12000;
}
}  // namespace DictionaryParserStub

class ChapterHtmlSlimParser {
 public:
  using CompletePage = std::function<void(std::unique_ptr<Page>, uint16_t, uint16_t, uint32_t)>;

  ChapterHtmlSlimParser(std::shared_ptr<Epub>, const std::string&, GfxRenderer&, int, float, bool, uint8_t, uint16_t,
                        uint16_t, bool, bool, bool, bool, const CompletePage& completePage, bool, const std::string&,
                        const std::string&, uint8_t)
      : completePage_(completePage) {}

  bool parseAndBuildPages() {
    if (!DictionaryParserStub::parseOk) return false;
    ESP.freeHeap = DictionaryParserStub::callbackFreeHeap;
    ESP.maxAllocHeap = DictionaryParserStub::callbackMaxAlloc;
    for (size_t i = 0; i < DictionaryParserStub::pagesToEmit; ++i) {
      auto page = std::make_unique<Page>();
      page->elements.resize(DictionaryParserStub::elementsPerPage);
      completePage_(std::move(page), 0, 0, 0);
    }
    return true;
  }

 private:
  CompletePage completePage_;
};
