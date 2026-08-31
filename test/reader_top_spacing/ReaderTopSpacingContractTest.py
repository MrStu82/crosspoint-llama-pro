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
assert "const int progressBarY = isTopEdge ? readerTopProgressBarY" in theme

# The change is top-edge-only; bottom placement and total viewport reservation
# stay byte-for-byte governed by the existing expressions.
assert "renderer.getScreenHeight() - orientedMarginBottom - sb.progressBarHeightPx" in theme
ui_theme = (root / "src/components/UITheme.cpp").read_text()
assert "sb.progressBarHeightPx + metrics.progressBarMarginTop" in ui_theme

print("PASS reader top geometry: bar y=0, text y=barHeight+1, bottom/reservation unchanged")
