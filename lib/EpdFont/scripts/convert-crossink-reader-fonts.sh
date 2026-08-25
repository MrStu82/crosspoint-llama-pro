#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# CrossInk-derived families compiled for the X4 Pro app-partition budget.
# Keep the optional built-ins deliberately bounded to Western prose plus seven
# music symbols, one heart, one face and one hand. Broader language/emoji
# coverage belongs to the existing SD .cpfont system; Noto remains the built-in
# multilingual fallback.
SIZES=(10 12 14 16)
STYLES=(Regular Bold Italic BoldItalic)
EMOJI_FONT=../builtinFonts/source/NotoEmoji/NotoEmoji-Regular.ttf
SYMBOLS_FONT=../builtinFonts/source/NotoSymbols/NotoSansSymbols-Regular.ttf
COMMON_ARGS=(
  --exact-intervals
  --additional-intervals 0x0020,0x007E
  --additional-intervals 0x00A0,0x00FF
  --additional-intervals 0x2000,0x206F
  --additional-intervals 0x20A0,0x20CF
  --additional-intervals 0x2190,0x21FF
  --additional-intervals 0xFB00,0xFB06
  --additional-intervals 0xFFFD,0xFFFD
  --additional-intervals 0x2669,0x266F
  --additional-intervals 0x2764,0x2764
  --additional-intervals 0x1F600,0x1F600
  --additional-intervals 0x1F44D,0x1F44D
  --font-include-intervals 1:0x2764,0x2764
  --font-include-intervals 1:0x1F600,0x1F600
  --font-include-intervals 1:0x1F44D,0x1F44D
  --font-include-intervals 2:0x2669,0x266F
  --2bit --compress --pnum --darken-aa
)

for family in LexendDeca Bitter; do
  family_lower=$(printf '%s' "$family" | tr '[:upper:]' '[:lower:]')
  for size in "${SIZES[@]}"; do
    for style in "${STYLES[@]}"; do
      style_lower=$(printf '%s' "$style" | tr '[:upper:]' '[:lower:]')
      name="${family_lower}_${size}_${style_lower}"
      python fontconvert.py "$name" "$size" \
        "../builtinFonts/source/${family}/${family}-${style}.ttf" \
        "$EMOJI_FONT" "$SYMBOLS_FONT" \
        "${COMMON_ARGS[@]}" > "../builtinFonts/${name}.h"
      printf 'Generated %s\n' "../builtinFonts/${name}.h"
    done
  done
done
