#pragma once

#include <cstdint>

struct EspDictionaryStub {
  uint32_t freeHeap = UINT32_MAX;
  uint32_t maxAllocHeap = UINT32_MAX;
  uint32_t getFreeHeap() const { return freeHeap; }
  uint32_t getMaxAllocHeap() const { return maxAllocHeap; }
};

inline EspDictionaryStub ESP;
inline unsigned long millis() { return 0; }
