#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])
main = (root / "src/main.cpp").read_text()
manager_h = (root / "src/activities/ActivityManager.h").read_text()
manager_cpp = (root / "src/activities/ActivityManager.cpp").read_text()
home_h = (root / "src/activities/home/HomeActivity.h").read_text()
home_cpp = (root / "src/activities/home/HomeActivity.cpp").read_text()

required = {
    "failed retained-frame load arms cleanup": "needsWakeRefresh = true;" in main,
    "routing forwards one-shot cleanup": "goHome(HomeMenuItem::NONE, needsWakeRefresh)" in main,
    "manager API carries cleanup": "bool cleanInitialRefresh = false" in manager_h,
    "manager forwards cleanup to Home": "initialMenuItem, cleanInitialRefresh" in manager_cpp,
    "Home stores cleanup immutably": "const bool cleanInitialRefresh;" in home_h,
    "first Home paint uses HALF":
        "cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH" in home_cpp,
}

failed = [name for name, ok in required.items() if not ok]
if failed:
    raise SystemExit("wake-cleanup contract failed: " + ", ".join(failed))

print("wake-cleanup contract PASS: missing sleep frame -> one clean Home paint; normal/Quick Resume unchanged")
