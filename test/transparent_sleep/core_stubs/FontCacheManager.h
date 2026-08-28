#pragma once

class FontCacheManager {
 public:
  void releaseSdFontCaches() { ++releaseCalls; }
  int releaseCalls = 0;
};
