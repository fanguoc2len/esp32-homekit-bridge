#!/usr/bin/env python3
"""Fail if tracked files contain local ESP-IDF Wi-Fi credentials."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]

CONFIG_PREFIX = "CONFIG_SMARTHOME_WIFI_"
SECRET_PATTERNS = (
    re.compile(CONFIG_PREFIX + r'PASSWORD="[^"]+"'),
    re.compile(CONFIG_PREFIX + r'SSID="[^"]+"'),
)

def tracked_files() -> list[pathlib.Path]:
    result = subprocess.run(
        ["git", "ls-files"],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return [ROOT / line for line in result.stdout.splitlines() if line]


def main() -> int:
    failures: list[str] = []

    for path in tracked_files():
        relative = path.relative_to(ROOT).as_posix()
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue

        for pattern in SECRET_PATTERNS:
            if pattern.search(text):
                failures.append(relative)
                break

    if failures:
        print("Tracked files contain Wi-Fi credential config:")
        for failure in failures:
            print(f"  - {failure}")
        print("Move local credentials to an ignored sdkconfig or menuconfig.")
        return 1

    print("No tracked Wi-Fi credentials found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
