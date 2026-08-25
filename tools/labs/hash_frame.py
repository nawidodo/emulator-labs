#!/usr/bin/env python3
"""Framebuffer hasher for emulator-labs golden-image tests.

Reads a raw RGBA8 framebuffer dump (or any binary blob) and prints its
SHA-256 over the raw bytes plus an FNV-1a 64-bit digest. The FNV digest is
what manifests reference; SHA-256 is printed for collision forensics.

Usage: hash_frame.py frame.rgba [--fnv-only]
"""

from __future__ import annotations

import argparse
import hashlib
import sys

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK = 0xFFFFFFFFFFFFFFFF


def fnv1a(data: bytes) -> str:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK
    return f"{h:016X}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("frame", help="raw framebuffer file")
    ap.add_argument("--fnv-only", action="store_true")
    args = ap.parse_args()
    data = open(args.frame, "rb").read()
    fnv = fnv1a(data)
    if args.fnv_only:
        print(fnv)
    else:
        sha = hashlib.sha256(data).hexdigest()
        print(f"FNV64 {fnv}\nSHA256 {sha}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
