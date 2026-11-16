# #picco-ftfont-1116 Phase 1 GATE (read-only, nothing built/flashed)
Branch feat/picco-fw-1115 HEAD e447d89a8 (SDK c431e72c).

## Integration point
- FreeType runtime TTF already linked on Picco: [env:picco] lib_deps FreeInkFont (+FREEINK_FONT_ENABLE_AUTOHINT) from TTF port b8ea003b9.
- Today: built-in reader fonts are prerendered .cpfont bitmaps (GfxRenderer::getGlyphBitmap GfxRenderer.cpp:63-68 → renderCharImpl :394/:504); TTF path is used only for SD /fonts (SdCardFontRegistry → SdCardFontSystem.cpp:449 ttf_->load, TtfEpdFont faults glyphs via vectorBitmapHandler EpdFontData.h:231).
- Single weight hook: lib/EpdFont/TtfEpdFont.cpp:161-165 RenderOptions → ro.embolden26_6 → FtFont.cpp:564-566 FT_Outline_Embolden (synthetic, no wght/fvar).
- Metrics: TtfEpdFont.cpp:342 fills EpdGlyph.advanceX from FreeType advance26_6; layout/pagination consume the same EpdGlyph/kern interface → no layout code change, but line breaks differ from .cpfont metrics (same as SD TTF today).

## Flash
- Slot ota_0 0xE70000 = 15,138,816 B; image 305 = 6,377,904 B; free 8,760,912 B.
- Estimate: NotoSerif R/B/I/BI TTF = 2,080,492 B → ~8.46 MB image, ~6.68 MB free. +NotoSans 4 = +2,546,680 B. Bitmap fonts stay (X4 Pro / UI).

## RAM
- Reader glyph cache PSRAM 1 MiB / 4096 glyphs (SdCardFontSystem.cpp:439-440), UI fallback 16 KiB/384 (:405). Picco n16r8 = 8 MiB PSRAM. FT face/heap from flash-mapped TTF (no copy) — live heap figure needs a device run.

## Fonts / licences (lib/EpdFont/builtinFonts/source)
- OFL file present: NotoSerif (+SemiBold), NotoSans, OpenDyslexic (.otf), NotoSansArabic/Hebrew. UFL: Ubuntu.
- NO licence file in repo: Bitter, LexendDeca, Caveat (OFL upstream, unverified here) → don't embed until licence copied.

## Decisions for Stuart
1. Families to embed (proposal: NotoSerif 4 styles only).
2. Picco default reader font = embedded TTF? (proposal yes; built-ins remain selectable).
3. Font Weight steps (proposal 0..4 → embolden26_6 0/16/32/48/64 = 0–1px).
4. Relation to Darker Text/Bold strokes enum (proposal: Font Weight applies to TTF only; Bold strokes stays for bitmap fonts).
5. Keep 2-bit 25/50/75 quantiser + autohint + stem darkening (proposal yes).
X4 Pro: all of it under FREEINK_DEVICE_PICCO → X4 Pro sections unchanged.
