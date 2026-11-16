#!/usr/bin/env python3
"""Prints how many production keys UPDATE_PUBLIC_KEYS holds.

  count-update-keys.py <UpdateKeys.h>

Reads the array itself, ignoring comments, so a commented-out or leftover entry isn't counted. It
also fails if the array's declared size doesn't match its entries, or an entry isn't 32 bytes. The
release job in .github/workflows/main.yml uses it to require two to five keys.
"""
from pathlib import Path
import re
import sys

KEY_SIZE = 32


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def count_keys(header_path):
    header = strip_comments(Path(header_path).read_text(encoding="utf-8"))
    array = re.search(
        r"std::array<\s*UpdateManifest::PublicKey\s*,\s*(\d+)\s*>\s*UPDATE_PUBLIC_KEYS\s*\{(.*?)\};",
        header, re.DOTALL)
    if array is None:
        sys.exit(f"Could not find UPDATE_PUBLIC_KEYS in {header_path}")

    declared = int(array.group(1))
    entries = re.findall(r"PublicKey\s*\{\s*\{(.*?)\}\s*\}", array.group(2), re.DOTALL)
    for number, entry in enumerate(entries, 1):
        size = len(re.findall(r"0x[0-9a-fA-F]{2}", entry))
        if size != KEY_SIZE:
            sys.exit(f"Key {number} in {header_path} is {size} bytes, not {KEY_SIZE}")
    if declared != len(entries):
        sys.exit(f"UPDATE_PUBLIC_KEYS is declared with {declared} keys but lists {len(entries)}")
    return len(entries)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    print(count_keys(sys.argv[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
