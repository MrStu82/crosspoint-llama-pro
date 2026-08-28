#!/usr/bin/env python3
"""Clean and rebuild a simulator environment without trusting prior objects."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", default="simulator_x4_pro")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    environment = args.environment
    configured = os.environ.get("PLATFORMIO_CMD")
    command = shlex.split(configured) if configured else [shutil.which("pio") or "pio"]

    subprocess.run(command + ["run", "-e", environment, "-t", "clean"], cwd=root, check=True)
    build_dir = root / ".pio" / "build" / environment
    stale_objects = list(build_dir.rglob("*.o")) if build_dir.exists() else []
    if stale_objects:
        raise SystemExit(f"clean left {len(stale_objects)} object files in {build_dir}")

    subprocess.run(command + ["run", "-e", environment], cwd=root, check=True)
    programs = [build_dir / "program", build_dir / "program.exe"]
    if not any(program.is_file() for program in programs):
        raise SystemExit(f"clean build did not link a simulator in {build_dir}")


if __name__ == "__main__":
    main()
