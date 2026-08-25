#!/usr/bin/env python3
"""Portable-core lint for emulator-labs.

Enforces the host/core boundary rule (curriculum §7 of the C17 foundations
track): a machine core must never include platform or UI headers. Frontends
belong outside the core; cores stay headless and portable.

Usage:
  check_portable_core.py [paths...]      # default: skels/ solutions/
Exit 0 iff no forbidden header appears in a core source file.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

FORBIDDEN = [
    r"<windows\.h>", r"<UIKit", r"<AppKit", r"<Metal/", r"<Direct3D",
    r"<d3d", r"<SDL", r"<GL/gl", r"<vulkan", r"<OpenAL", r"<AudioUnit",
    r"<CoreAudio", r"<X11/", r"<wayland",
]

PATTERN = re.compile("|".join(FORBIDDEN), re.IGNORECASE)

# Portable-core invariants beyond headers (comprehensive review #26):
# no host wall-clock inside machine state, no raw pointer serialization.
RUNTIME_PATTERNS = [
    (re.compile(r"\btime\s*\(\s*(?:nullptr|0|NULL)\s*\)"),
     "host wall-clock time()"),
    (re.compile(r"chrono::system_clock"), "host system clock"),
    (re.compile(r"reinterpret_cast<[^>]*char\s*\*>[^;]*memcpy"),
     "raw pointer serialization"),
]
EXTS = {".c", ".h", ".cc", ".cpp", ".hpp"}


def main() -> int:
    roots = sys.argv[1:] or ["skels", "solutions"]
    bad: list[tuple[Path, int]] = []
    scanned = 0
    for root in roots:
        p = Path(root)
        if not p.is_dir():
            continue
        for f in sorted(p.rglob("*")):
            if f.suffix.lower() not in EXTS:
                continue
            scanned += 1
            try:
                text = f.read_text(errors="replace")
            except OSError:
                continue
            for lineno, line in enumerate(text.splitlines(), 1):
                if PATTERN.search(line):
                    bad.append((f, lineno))
                    print(f"{f}:{lineno}: forbidden platform/UI header "
                          f"-> {line.strip()}")
                elif f.suffix.lower() != ".h":
                    for pat, what in RUNTIME_PATTERNS:
                        if pat.search(line):
                            print(f"{f}:{lineno}: portability: {what} "
                                  f"-> {line.strip()}")
                            break
    print(f"[lint-portable] scanned {scanned} files under {roots}; "
          f"{len(bad)} violations")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
