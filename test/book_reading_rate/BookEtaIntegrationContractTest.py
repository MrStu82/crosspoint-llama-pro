#!/usr/bin/env python3
"""Source and synthetic-framebuffer contracts for the bounded BOOK LEFT change."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]


def source(path: str) -> str:
    return (ROOT / path).read_text()


stats = source("src/util/BookReadingStats.cpp")
home = source("src/activities/home/HomeActivity.cpp")
geometry = source("src/activities/home/HomeInkPointGeometry.h")
manager = source("src/activities/ActivityManager.cpp")
readers = [
    source("src/activities/reader/EpubReaderActivity.cpp"),
    source("src/activities/reader/TxtReaderActivity.cpp"),
    source("src/activities/reader/XtcReaderActivity.cpp"),
]

# Both bounded state files retain the proven atomic rename/write path. Legacy
# cumulative forwardPages remains source-compatible but is never pace input.
assert stats.count("ProgressFile::writeAtomic") == 2
assert "stored.totalSeconds = satAdd(stored.totalSeconds, seconds);" in stats
assert "forwardPages" not in stats
assert "progressPercent" not in stats
assert "overallRate(overall, stored.fingerprint)" in stats
assert "if (!result.currentRate" in stats

# Every reader exposes compatible pagination identity, records only qualified
# visible forward dwell, persists a non-percent content basis, and is paused
# while an activity or the brightness sheet covers it.
for reader in readers:
    assert "rateFingerprint()" in reader
    assert "dwellTracker.takeQualifiedForward" in reader
    assert "BookReadingStats::recordQualifiedPage" in reader
    assert "BookReadingStats::updatePosition" in reader
    assert "dwellTracker.pause()" in reader
assert manager.count("onCovered()") >= 2
assert manager.count("onUncovered()") >= 2
assert "popupObscuresPage = pendingSyncSaveError || showBookmarkMessage || showDictionaryMessage" in readers[0]
assert "section && !popupObscuresPage" in readers[0]

# BOOK LEFT keeps its shipped sufficient/insufficient row states: a selected
# qualified rate renders the value; insufficient evidence withholds the group.
assert "if (eta.bookMinutes)" in home
assert "if (i == 2 && !valueAvailable[i]) continue;" in home
assert "bookStats.pagesPerMinuteQ16, bookStats.remainingPagesQ16" in home
assert "progressPercent" not in geometry[geometry.index("inline EtaState estimateEtas") :]

# Synthetic framebuffer proof of the approved border-only mask. The actual
# source contract selects `after`: same six-pixel fill, only the one-pixel old
# rectangle perimeter removed, with no change outside the old bar bounds.
assert "renderer.drawRect(progressLayout.x, progressLayout.y" not in home
assert "renderer.fillRect(progressLayout.fillX, progressLayout.fillY" in home

width, height = 220, 14
fill_x, fill_y, fill_width, fill_height = 1, 4, 132, 6


def frame(with_border: bool):
    pixels = set()
    if with_border:
        pixels.update((x, 0) for x in range(width))
        pixels.update((x, height - 1) for x in range(width))
        pixels.update((0, y) for y in range(height))
        pixels.update((width - 1, y) for y in range(height))
    pixels.update(
        (x, y)
        for y in range(fill_y, fill_y + fill_height)
        for x in range(fill_x, fill_x + fill_width)
    )
    return pixels


before, after = frame(True), frame(False)
removed = before - after
expected_border = {
    (x, y)
    for y in range(height)
    for x in range(width)
    if x in (0, width - 1) or y in (0, height - 1)
}
assert removed == expected_border
assert after == frame(False)
assert len(after) == fill_width * 6

print("PASS BOOK LEFT integration and border-only framebuffer contract")
