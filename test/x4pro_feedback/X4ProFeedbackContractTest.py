#!/usr/bin/env python3
"""Deterministic source/geometry contract for the Aug-29 X4 Pro feedback pass."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]

home = (root / "src/activities/home/HomeActivity.cpp").read_text()
geometry = (root / "src/activities/home/HomeInkPointGeometry.h").read_text()
shell = (root / "src/components/InkPointShell.cpp").read_text()
manager = (root / "src/activities/ActivityManager.cpp").read_text()
reader = (root / "src/activities/reader/ReaderUtils.h").read_text()
text_settings = (root / "src/activities/settings/TextSettingsActivity.cpp").read_text()
controls = (root / "src/components/ControlCenterModel.h").read_text()
wifi = (root / "src/activities/network/WifiSelectionActivity.cpp").read_text()
transfer = (root / "src/activities/network/CrossPointWebServerActivity.cpp").read_text()
usb = (root / "src/activities/settings/UsbTransferActivity.cpp").read_text()

# Home has no drawn cover progress or focus outline; book ETA is a full row below chapter.
for retired in ["coverProgressLayout", "kCoverProgressHeight", "coverX - 3"]:
    assert retired not in home, retired
assert "rightX, statStart + 2 * statStep" in geometry

# Battery baseline moves by exactly two panel rows; cog path is a symmetric 24-point path.
assert "battery), 4, battery" in shell
assert "constexpr int ox[24]" in shell and "constexpr int oy[24]" in shell

# Quick Settings owns top/down exclusively. Reader Text Settings owns bottom/up.
assert "mappedInput.wasMenuGesture()" in manager
assert "statusBarTap" not in manager
assert "mappedInput.wasBrightnessSheetGesture()" not in manager
assert "input.wasBrightnessSheetGesture()" in reader
assert "input.wasMenuGesture()" not in reader

# The Text Settings window is bottom-aligned and dismisses from the exposed top band.
assert "DrawerChrome::Edge::Bottom" in text_settings
assert "displayWindow(0, drawerTop" in text_settings
assert "drawerTop + metrics_.topPadding" in text_settings

# Tiles are exactly 2/3 of 84px; labels have a measured gap and the panel derives its height.
assert "kTileHeight = 56" in controls
assert "kCaptionToSliderGap = 8" in controls
assert "contentsBottom + kBottomPadding" in controls

# All three transfer/connect surfaces use the approved X4 Pro shell; existing touch
# rows/prompts remain in WifiSelection and the new USB/web exit surfaces are tappable.
assert "InkPointShell::drawHeader(renderer, \"Wi-Fi\")" in wifi
assert "handleListTouch" in wifi and "colTouch" in wifi
assert "InkPointShell::drawHeader" in transfer and "wasScreenTapped" in transfer
assert "InkPointShell::drawHeader(renderer, \"USB transfer\")" in usb and "wasScreenTapped" in usb

print("PASS X4 Pro Aug-29 geometry/gesture/touch/style contract")
