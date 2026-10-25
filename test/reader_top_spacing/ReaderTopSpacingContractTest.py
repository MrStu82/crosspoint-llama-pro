#!/usr/bin/env python3
"""Exact source geometry contract for the reader's top status bar prototype."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
theme = (root / "src/components/themes/BaseTheme.cpp").read_text()

assert "constexpr int readerTopProgressBarY = 0;" in theme
assert "constexpr int readerTopStatusGap = 1;" in theme

# With a visible top progress bar, its fill occupies y=[0, height-1] and the
# status text layout begins at y=height+1: one blank row, no collision. The
# effective height includes the existing fillMargin extension used by readers.
assert "const int progressBarHeight = sb.progressBarHeightPx + (fillMargin ? orientedMarginBottom - 1 : 0);" in theme
assert "readerTopProgressBarY + progressBarHeight + readerTopStatusGap" in theme
assert "renderer.fillRect(barMarginLeft, progressBarY, barWidth, progressBarHeight, true);" in theme
assert "const int progressBarY = isTopEdge ? topInset + readerTopProgressBarY" in theme

# Top bezel offset: every top-edge y (bar and text lane) shifts by topInset, which
# readers pass as readerTopBezelOffset() (0 by default and outside portrait).
assert "? topInset + readerTopProgressBarY + progressBarHeight + readerTopStatusGap" in theme
assert ": topInset + orientedMarginTop + paddingBottom" in theme
settings = (root / "src/CrossPointSettings.h").read_text()
assert "uint8_t topBezelOffset = 0;" in settings
assert "return orientation == PORTRAIT ? topBezelOffset : 0;" in settings
epub = (root / "src/activities/reader/EpubReaderActivity.cpp").read_text()
txt = (root / "src/activities/reader/TxtReaderActivity.cpp").read_text()
assert epub.count("SETTINGS.readerTopBezelOffset()") == 3  # text top, dictionary geometry, top bar
assert "orientedMarginTop += SETTINGS.readerTopBezelOffset();" in epub
assert "Edge::TOP, SETTINGS.readerTopBezelOffset());" in epub
assert txt.count("SETTINGS.readerTopBezelOffset()") == 2  # text top, top bar
assert "cachedOrientedMarginTop += cachedScreenMargin + SETTINGS.readerTopBezelOffset();" in txt
assert "Edge::TOP, SETTINGS.readerTopBezelOffset());" in txt

# The change is top-edge-only; bottom placement and total viewport reservation
# stay byte-for-byte governed by the existing expressions.
assert "renderer.getScreenHeight() - orientedMarginBottom - sb.progressBarHeightPx" in theme
ui_theme = (root / "src/components/UITheme.cpp").read_text()
assert "sb.progressBarHeightPx + metrics.progressBarMarginTop" in ui_theme

print("PASS reader top geometry: bar y=bezel, text y=bezel+barHeight+1, text top +bezel, bottom/reservation unchanged")
