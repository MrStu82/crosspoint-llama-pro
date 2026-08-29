#!/usr/bin/env python3
"""Deterministic contract for the X4 Pro's supported prebuilt core profile."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def section(name: str) -> str:
    match = re.search(
        rf"^\[{re.escape(name)}\]\n(.*?)(?=^\[|\Z)",
        PIO,
        flags=re.MULTILINE | re.DOTALL,
    )
    assert match, f"missing [{name}]"
    return match.group(1)


base = section("base")
tuned = section("firmware_tuned")
x4pro = section("env:x4pro")

assert "custom_sdkconfig" not in base
assert "custom_component_remove" not in base
assert "custom_sdkconfig" in tuned
assert "custom_component_remove" in tuned

assert re.search(r"^extends\s*=\s*base\s*$", x4pro, re.MULTILINE)
assert "firmware_tuned" not in x4pro
assert "CONFIG_TINYUSB_" not in x4pro
assert "fix_tinyusb_include.py" not in x4pro
assert "post:scripts/stamp_app_desc.py" in x4pro

assert "-DARDUINO_USB_MODE=1" in base
assert "-DARDUINO_USB_CDC_ON_BOOT=1" in base
assert "pre:scripts/patch_pioarduino_cache.py" in base

for profile in ("default", "gh_release", "gh_release_rc", "slim", "sticky"):
    assert re.search(
        r"^extends\s*=\s*base,\s*firmware_tuned\s*$",
        section(f"env:{profile}"),
        re.MULTILINE,
    ), f"{profile} lost the existing tuned core profile"

print("X4 Pro platform contract: PASS")
