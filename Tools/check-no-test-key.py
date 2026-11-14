#!/usr/bin/env python3
"""Fails if a release package contains the updater's test key.

  check-no-test-key.py <UpdateKeys.h> <package.zip> [<package.zip> ...]

A build made with RIO_UPDATER_TEST_KEYS trusts only the throwaway test key, so it must never be
released. This looks for the key's bytes in the finished packages rather than at how the build was
configured, so it catches every way the option could end up on. See docs/autoupdate_overview.md,
"Keys". The release job in .github/workflows/main.yml runs it on every package that has an updater.
"""
from pathlib import Path
import re
import sys
import zipfile

KEY_SIZE = 32


def read_test_key(header_path):
    header = Path(header_path).read_text(encoding="utf-8")
    block = re.search(r"UPDATE_TEST_PUBLIC_KEYS\{\{(.*?)\}\};", header, re.DOTALL)
    if block is None:
        sys.exit(f"Could not find UPDATE_TEST_PUBLIC_KEYS in {header_path}")

    key = bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-fA-F]{2})", block.group(1)))
    if len(key) != KEY_SIZE:
        sys.exit(f"Expected a {KEY_SIZE} byte test key in {header_path}, found {len(key)} bytes")
    return key


def is_updater(name):
    return name.endswith(("Updater.exe", "Contents/MacOS/Project Rio Updater"))


def check_package(path, key):
    """Returns the problems found in one package, an empty list if it is clean."""
    found_updater = False
    problems = []
    with zipfile.ZipFile(path) as package:
        for entry in package.infolist():
            if entry.is_dir():
                continue
            found_updater = found_updater or is_updater(entry.filename)
            if key in package.read(entry):
                problems.append(f"{path}: {entry.filename} contains the test key")

    # Scanning a package with no updater in it would pass without proving anything.
    if not found_updater:
        problems.append(f"{path}: no updater found")
    return problems


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)

    key = read_test_key(sys.argv[1])
    problems = [problem for path in sys.argv[2:] for problem in check_package(path, key)]
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1

    print(f"No test key in {len(sys.argv) - 2} package(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
