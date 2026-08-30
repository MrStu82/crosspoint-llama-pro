#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

settings = (ROOT / "src/CrossPointSettings.h").read_text()
settings_list = (ROOT / "src/SettingsList.h").read_text()
utils = (ROOT / "src/activities/reader/ReaderUtils.h").read_text()
activity = (ROOT / "src/activities/reader/ReaderToolsActivity.cpp").read_text()
header = (ROOT / "src/activities/reader/ReaderToolsActivity.h").read_text()
epub = (ROOT / "src/activities/reader/EpubReaderActivity.cpp").read_text()
txt = (ROOT / "src/activities/reader/TxtReaderActivity.cpp").read_text()
xtc = (ROOT / "src/activities/reader/XtcReaderActivity.cpp").read_text()

assert "OFF = 0" in settings and "CHAPTER_SKIP = 1" in settings and "ORIENTATION_CHANGE = 2" in settings
assert "READER_TOOLS = 3" in settings
assert "STR_LONG_PRESS_BEHAVIOR_ORIENTATION, StrId::STR_READER_TOOLS" in settings_list

assert "SETTINGS.longPressButtonBehavior == CrossPointSettings::READER_TOOLS" in utils
assert "(touch.prev || touch.next) && touch.heldMs > SKIP_HOLD_MS" in utils
assert "previousZoneWidth = width / 3" in utils  # existing centre/right short-tap mapping remains

ordered = ["GoToPercent", "AddBookmark", "Bookmarks", "Dictionary", "KOReaderSync", "TextSettings"]
positions = [activity.index(f"add(Action::{name}") for name in ordered]
assert positions == sorted(positions)
assert "if (format == Format::Epub)" in activity
assert "if (format != Format::Xtc)" in activity
assert "std::array<Item, 6>" in header

assert "fillRoundedRect(shadowX" in activity and "fillRoundedRect(panel.x" in activity
assert "renderer.clearScreen" not in activity  # underlying reader framebuffer is retained
assert "renderer.displayWindow(panel.x, panel.y" in activity  # controller pixels outside the overlay are untouched
assert "ROW_HEIGHT = 70" in (ROOT / "src/activities/reader/ReaderToolsGeometry.h").read_text()
assert "PANEL_WIDTH = 412" in (ROOT / "src/activities/reader/ReaderToolsGeometry.h").read_text()
assert "PANEL_HEIGHT_EPUB = 510" in (ROOT / "src/activities/reader/ReaderToolsGeometry.h").read_text()
assert "!ReaderToolsGeometry::contains(panel, x, y)" in activity
assert "handleHomeGesture" in activity and "closeCancelled();" in activity

for source in (epub, txt, xtc):
    assert "ReaderUtils::shouldOpenReaderTools(touch)" in source
    assert "openReaderTools();" in source
assert "MenuAction::GO_TO_PERCENT" in epub
assert "MenuAction::BOOKMARKS" in epub
assert "MenuAction::DICTIONARY" in epub
assert "MenuAction::SYNC" in epub
assert "MenuAction::TEXT_SETTINGS" in epub
assert "if (!currentPageBookmarked) addBookmark();" in epub
assert "ReaderToolsActivity::Format::Txt" in txt and "TextSettingsActivity" in txt
assert "ReaderToolsActivity::Format::Xtc" in xtc and "GoToPercent" in xtc

print("PASS Reader Tools setting, long-hold arbitration, format filters, geometry, dismissal and routes")
