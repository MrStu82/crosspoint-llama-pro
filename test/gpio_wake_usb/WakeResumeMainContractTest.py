#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])
main = (root / "src/main.cpp").read_text()
gpio = (root / "lib/hal/HalGPIO.h").read_text()

required = {
    "sleep persists splashless one-shot": "APP_STATE.showBootScreen = false;" in main,
    "wake immediately rearms ordinary splash": "APP_STATE.showBootScreen = true;" in main,
    "only persisted verified wake is splashless": "useSplashlessWake" in main
        and "gpio_policy::shouldUseSplashlessWake" in main,
    "reset or panic rearms stale one-shot": "gpio_policy::shouldRearmBootScreen" in main,
    "ordinary splash route remains": "case BootResume::Splash:" in main
        and "activityManager.goToBoot();" in main,
    "panic route remains": "HalSystem::isRebootFromPanic()" in main
        and "activityManager.goToCrashReport();" in main,
    "stale quick-resume frame is removed": "Storage.remove(SLEEP_FRAME_FILE);" in main,
    "wake gate is armed only for power wake":
        "wakePowerInputGate.arm(wakeupReason == HalGPIO::WakeupReason::PowerButton);" in main,
    "wake release is swallowed before activity dispatch":
        "wakePowerInputGate.consumeWakeRelease(powerPressed)" in main,
    "wake hold cannot trigger screenshot combo":
        "wakePowerInputGate.allowsPowerActions() && powerPressed" in main,
    "X4 GT911 Home tap remains exposed": "wasHomeKeyTapped() const;" in gpio,
    "X4 GT911 Home hold remains exposed": "wasHomeKeyLongPressed() const;" in gpio,
}

failed = [name for name, ok in required.items() if not ok]
if failed:
    raise SystemExit("wake-resume contract failed: " + ", ".join(failed))

print("wake-resume contract PASS: splashless one-shot, safe splash/panic rails, power-only release gate, GT911 intact")
