#!/usr/bin/env python3
"""Fail if repository files contain local ESP-IDF Wi-Fi credentials."""

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

FALLBACK_SKIP_DIRS = {
    ".git",
    "__pycache__",
    ".pytest_cache",
}


def fallback_files() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []

    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue

        relative_parts = path.relative_to(ROOT).parts
        if any(part in FALLBACK_SKIP_DIRS for part in relative_parts):
            continue
        if relative_parts and relative_parts[0].startswith("build"):
            continue

        files.append(path)

    return sorted(files)


def tracked_files() -> list[pathlib.Path]:
    try:
        result = subprocess.run(
            ["git", "ls-files"],
            cwd=ROOT,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        print(
            "warning: could not query tracked files with git; "
            f"falling back to a repository walk ({exc}).",
            file=sys.stderr,
        )
        return fallback_files()

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
        print("Repository files contain Wi-Fi credential config:")
        for failure in failures:
            print(f"  - {failure}")
        print("Move local credentials to an ignored sdkconfig or menuconfig.")
        return 1

    print("No tracked Wi-Fi credentials found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
