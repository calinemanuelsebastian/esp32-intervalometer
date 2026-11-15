#!/usr/bin/env python3
"""Fail if the two packaged editions' shared firmware core has drifted."""
import argparse
import subprocess
import sys
from pathlib import Path

CORE = ("TimerPlatform.h", "PulseTimer.cpp", "PulseTimer.h", "Sequence.h", "Input.h", "Controls.h", "Capture.h", "Exposure.h")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test", action="store_true", help="also run both host test suites")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    editions = [root / name / "R7Intervalometer" for name in ("OLED", "TFT")]
    errors = []
    for name in CORE:
        pair = [edition / name for edition in editions]
        missing = [str(path) for path in pair if not path.is_file()]
        if missing:
            errors.append("Missing: " + ", ".join(missing))
        elif pair[0].read_bytes() != pair[1].read_bytes():
            errors.append("Core differs between OLED and TFT: " + name)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("PASS: all eight shared core files match between OLED and TFT.", flush=True)
    if args.test:
        for edition in editions:
            print("Testing " + edition.parent.name, flush=True)
            subprocess.run([sys.executable, str(edition / "tests" / "run_host_tests.py")], check=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
