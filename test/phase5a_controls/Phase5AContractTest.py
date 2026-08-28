#!/usr/bin/env python3
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
sheet = (root / "src/components/BrightnessSheet.cpp").read_text()
keyboard = (root / "src/activities/util/KeyboardEntryActivity.cpp").read_text()
reveal = (root / "src/activities/util/TransientPasswordReveal.h").read_text()
manager = (root / "src/activities/ActivityManager.cpp").read_text()
settings = (root / "src/SettingsList.h").read_text()

required_sheet = [
    "ControlCenterModel::kBrightnessMin",
    "ControlCenterModel::kWarmthMin",
    "STR_BRIGHTNESS",
    "STR_WARM_COOL_BALANCE",
    "STR_NIGHT_MODE",
    "STR_FORCE_REFRESH",
    "STR_TOUCH_TOGGLE",
    "promoteNextRefresh(HalDisplay::FULL_REFRESH)",
    "ReaderUtils::applyOrientation",
]
for token in required_sheet:
    assert token in sheet, token
assert "{ControlCenterModel::kBrightnessMin" in settings
assert "{ControlCenterModel::kWarmthMin" in settings
assert "statusBarTap" in manager and "wasBrightnessSheetGesture" in manager

# Reveal is contact-scoped and every lifecycle/cancellation path remasks.
for token in ["passwordReveal.begin()", "passwordReveal.reset()", "passwordRevealTouchActive", "onExit()"]:
    assert token in keyboard, token
assert "revealPos" not in keyboard
assert "isPassword && !passwordReveal.visible() ? displayCursorChar : cursorChar" in keyboard
assert "std::string" not in reveal
assert "save" not in reveal.lower()
assert "log" not in reveal.lower()

# Phase 5A is controls-only: Home implementation and shell must remain byte-for-byte
# at the approved Phase 4B base.
subprocess.run([
    "git", "diff", "--exit-code", "d3bf7a084ac90c58429a0338a2003b87e536b4a0", "--",
    "src/activities/home", "src/components/InkPointShell.cpp", "src/components/InkPointShell.h"
], cwd=root, check=True, stdout=subprocess.DEVNULL)
print("PASS phase5a source/privacy/home-delta contract")
