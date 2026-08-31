#!/usr/bin/env python3
"""Exact source geometry contract for the reader's top status bar prototype."""

from pathlib import Path


root = Path(__file__).resolve().parents[2]
theme = (root / "src/components/themes/BaseTheme.cpp").read_text()

assert "constexpr int readerTopStatusLift = 8;" in theme

# The progress bar keeps its production inset and effective thickness exactly;
# only the top text lane moves up eight pixels.
assert "? orientedMarginTop + paddingBottom" in theme
assert "metrics.statusBarVerticalMargin - 4 - readerTopStatusLift" in theme
assert "const int barHeight = sb.progressBarHeightPx + (fillMargin ? orientedMarginBottom - 1 : 0);" in theme
assert "renderer.fillRect(barMarginLeft, progressBarY, barWidth, barHeight, true);" in theme

# The change is top-edge-only; bottom placement and total viewport reservation
# stay byte-for-byte governed by the existing expressions.
assert "renderer.getScreenHeight() - orientedMarginBottom - sb.progressBarHeightPx" in theme
ui_theme = (root / "src/components/UITheme.cpp").read_text()
assert "sb.progressBarHeightPx + metrics.progressBarMarginTop" in ui_theme

print("PASS reader top geometry: production bar unchanged, status lift=8px, bottom/reservation unchanged")
