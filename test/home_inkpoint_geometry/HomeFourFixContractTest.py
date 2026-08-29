#!/usr/bin/env python3
"""Source contract for the four bounded X4 Pro Home corrections."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
home = (ROOT / "src/activities/home/HomeActivity.cpp").read_text()
geometry = (ROOT / "src/activities/home/HomeInkPointGeometry.h").read_text()
shell = (ROOT / "src/components/InkPointShell.cpp").read_text()

# Progress is derived from exact cover bounds, while the approved no-focus-ring
# contract and the existing full cover lane tap path remain intact.
assert "coverProgressLayout(\n      coverX, coverY, coverW, coverH" in home
assert "renderer.drawRect(progressLayout.x, progressLayout.y" not in home
assert "renderer.fillRect(progressLayout.fillX, progressLayout.fillY" in home
assert "progressLayout.fillWidth, progressLayout.fillHeight" in home
assert "if (inkPointFocus == 0" not in home
assert "x >= kInkCoverLane.x && x < kInkCoverRight" in home
assert "kCoverProgressGap = 0" in geometry
assert "kCoverProgressFillHeight = 6" in geometry

# The chevron moves only through its measured gap; Stats bounds and activation
# remain unchanged.
assert "kStatsToChevronGap = 8" in geometry
assert "const Rect kInkStats{252, 300, 208, 250};" in home
assert "case 7: onStatsOpen();" in home

# Header percentage moved from the shipped y=4 to exactly y=5.
assert re.search(r"drawText\(SMALL_FONT_ID, 460 - .*battery\), 5, battery\);", shell)

def coordinates(name: str):
    match = re.search(rf"{name}\s*=\s*\{{(.*?)\}};", shell, re.DOTALL)
    assert match, name
    return [int(value) for value in re.findall(r"-?\d+", match.group(1))]


x = coordinates("kSettingsCogX")
y = coordinates("kSettingsCogY")
assert len(x) == len(y) == 32
points = set(zip(x, y))
assert all((-px, py) in points and (px, -py) in points for px, py in points)
assert min(x) == -14 and max(x) == 14 and min(y) == -12 and max(y) == 12
assert "fillPolygon(x, y" in shell
assert "cx - 4, cy - 4, 8, 8, 4" in shell

# Footer geometry/tap rectangles are untouched.
assert "constexpr int kFooterX = 14;" in shell
assert "constexpr int kTabWidth = 72;" in shell
assert "constexpr int kTabGap = 4;" in shell
assert "x >= left && x < left + kTabWidth" in shell

print("PASS X4 Pro Home four-fix geometry/source contract")
