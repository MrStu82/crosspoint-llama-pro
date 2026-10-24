#pragma once

#include <cstdint>

// Block elements (U+2580-259F: full/half blocks, shades) are in no reader font. U+2588 is used
// as a redaction bar in EPUBs; without this it falls back to the U+FFFD diamond. A missing block
// element (and U+25A0 black square) is drawn as the replacement glyph's box filled solid, keeping
// its advance.
constexpr bool isBlockElement(uint32_t cp) { return (cp >= 0x2580 && cp <= 0x259F) || cp == 0x25A0; }
