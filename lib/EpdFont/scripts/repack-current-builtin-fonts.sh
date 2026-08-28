#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Lossless migration for checked-in generated headers. Unlike rasterisation, this
# preserves the exact existing glyph set and visual metrics while changing only
# DEFLATE encoding and kerning storage.
headers=()
for family in notoserif notosans; do
  for size in 12 14 16 18; do
    for style in regular italic bold bolditalic; do
      headers+=("../builtinFonts/${family}_${size}_${style}.h")
    done
  done
done
headers+=("../builtinFonts/notosans_8_regular.h")
for size in 10 12; do
  for style in regular bold; do
    headers+=("../builtinFonts/ubuntu_${size}_${style}.h")
  done
done

python repack-builtin-font.py "${headers[@]}"
python verify_compression.py ../builtinFonts/
