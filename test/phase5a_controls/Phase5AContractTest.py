#!/usr/bin/env python3
from pathlib import Path

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
assert "statusBarTap" not in manager
assert "mappedInput.wasMenuGesture()" in manager

# Reveal is contact-scoped and every lifecycle/cancellation path remasks.
for token in ["passwordReveal.begin()", "passwordReveal.reset()", "passwordRevealTouchActive", "onExit()"]:
    assert token in keyboard, token
assert "revealPos" not in keyboard
assert "isPassword && !passwordReveal.visible() ? displayCursorChar : cursorChar" in keyboard
assert "std::string" not in reveal
assert "save" not in reveal.lower()
assert "log" not in reveal.lower()

# Home and shell now carry the separately gated Aug-29 X4 Pro feedback delta.
# Keep this contract focused on Phase 5A controls/privacy; the deterministic
# x4pro_feedback contract owns that later surface.
print("PASS phase5a source/privacy contract")
