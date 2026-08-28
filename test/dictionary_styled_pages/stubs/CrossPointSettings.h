#pragma once

#include <cstdint>

struct DictionarySettingsStub {
  bool extraParagraphSpacing = true;
  uint8_t paragraphAlignment = 0;
  bool hyphenationEnabled = true;
  bool focusReadingEnabled = false;
  bool guideReadingEnabled = false;
  bool forceParagraphIndents = false;

  int getReaderFontId() const { return 1; }
  float getReaderLineCompression() const { return 1.0f; }
};

inline DictionarySettingsStub SETTINGS;
