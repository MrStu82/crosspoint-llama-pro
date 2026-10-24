#pragma once

#include <cstdint>

// Codepoints missing from the built-in reader fonts are drawn as a close glyph the fonts do have,
// instead of the U+FFFD diamond. Greek has no glyphs at all, so it is transliterated.
// Returns 0 when there is no substitute.
constexpr uint32_t glyphSubstitute(const uint32_t cp) {
  switch (cp) {
    case 0x0101: return 'a';     // a with macron
    case 0x011B: return 'e';     // e with caron
    case 0x02BB: return 0x2018;  // okina -> left single quote
    case 0x2003: return ' ';     // em space
    default: break;
  }
  // Greek capitals U+0391-03A9 and small letters U+03B1-03C9 (U+03A2/03C2 final sigma -> s).
  constexpr char kGreek[] = "ABGDEZHQIKLMNXOPRsSTYFCYW";
  if (cp >= 0x0391 && cp <= 0x03A9) return static_cast<uint8_t>(kGreek[cp - 0x0391]);
  if (cp >= 0x03B1 && cp <= 0x03C9) {
    const char c = kGreek[cp - 0x03B1];
    return static_cast<uint8_t>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
  }
  return 0;
}
