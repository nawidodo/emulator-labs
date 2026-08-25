#!/usr/bin/env python3
"""Chapter-gate progress tracker for emulator-labs (curriculum §4, §60).

Gate rule: chapter N+1 unlocks only when ALL five components of chapter N
(exercises, starter, debug, challenge, coding_test) are 'passed'.

Usage:
  progress.py status
  progress.py mark <chapter> <component> passed|failed|active
  progress.py unlock-check <chapter>          # exit 0 if unlocked
Components: exercises starter debug challenge coding_test
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

COMPONENTS = ["exercises", "starter", "debug", "challenge", "coding_test"]
GATE_MARK = "passed"


def repo() -> Path:
    return Path(__file__).resolve().parents[2]


def load() -> dict:
    return json.loads((repo() / "progress.json").read_text())


def save(data: dict) -> None:
    (repo() / "progress.json").write_text(json.dumps(data, indent=2) + "\n")


def chapters_in_order(data: dict) -> list[str]:
    return sorted(data["chapters"].keys())


def is_unlocked(data: dict, ch: str) -> bool:
    order = chapters_in_order(data)
    if ch not in order:
        return False
    i = order.index(ch)
    if i == 0:
        return True
    prev = data["chapters"][order[i - 1]]
    return all(prev["components"][c] == GATE_MARK for c in COMPONENTS)


def cmd_status(_: argparse.Namespace) -> int:
    data = load()
    cur = data["student"]["current"]
    print("# Emulator School — gated laboratory curriculum\n")
    print(f"Current Chapter: {cur}")
    unlocked = [c for c in chapters_in_order(data) if is_unlocked(data, c)]
    print(f"Highest Unlocked Chapter: "
          f"{max(unlocked, key=lambda c: chapters_in_order(data).index(c))}"
          f"\n")
    hdr = f"| {'Chapter':<38} | " + " | ".join(f"{c[:9]:>9}" for c in COMPONENTS) \
        + " | Status  |"
    print(hdr)
    print("|" + "-" * (len(hdr) - 2) + "|")
    for ch in chapters_in_order(data):
        st = data["chapters"][ch]
        cells = []
        all_pass = True
        for c in COMPONENTS:
            v = st["components"][c]
            cells.append({"passed": "     ✓   ", "failed": "   ✗    ",
                          "active": "  ~     ", "": "    -    "}.get(v,
                          f"{v:>9}"))
            all_pass &= v == GATE_MARK
        state = "PASSED" if all_pass else (
            "ACTIVE" if ch == cur else ("unlocked" if is_unlocked(data, ch)
                                        else "🔒 LOCKED"))
        print(f"| {ch:<38} | " + " | ".join(cells) + f" | {state} |")
    return 0


def cmd_mark(ns: argparse.Namespace) -> int:
    data = load()
    ch = ns.chapter
    if ch not in data["chapters"]:
        sys.exit(f"error: unknown chapter '{ch}'")
    if not is_unlocked(data, ch):
        sys.exit(f"error: {ch} is LOCKED — pass the previous chapter gate first")
    comp = ns.component
    where = "components"
    if comp not in COMPONENTS:
        sys.exit(f"error: component must be one of {COMPONENTS}")
    data["chapters"][ch][where][comp] = ns.state
    # Advance pointer when the whole gate passes.
    comps = data["chapters"][ch][where]
    if all(comps[c] == GATE_MARK for c in COMPONENTS):
        order = chapters_in_order(data)
        i = order.index(ch)
        if i + 1 < len(order):
            nxt = order[i + 1]
            data["student"]["current"] = nxt
            print(f"GATE PASSED: {ch} -> {nxt} ACTIVE")
        else:
            print(f"GATE PASSED: {ch} (final chapter)")
    save(data)
    print(f"marked {ch}/{comp} = {ns.state}")
    return 0


def cmd_unlock_check(ns: argparse.Namespace) -> int:
    data = load()
    ok = is_unlocked(data, ns.chapter)
    print(f"{ns.chapter}: {'UNLOCKED' if ok else 'LOCKED'}")
    return 0 if ok else 2


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    m = sub.add_parser("mark")
    m.add_argument("chapter")
    m.add_argument("component", choices=COMPONENTS)
    m.add_argument("state", choices=["passed", "failed", "active", ""])
    u = sub.add_parser("unlock-check")
    u.add_argument("chapter")
    args = ap.parse_args()
    return {"status": cmd_status, "mark": cmd_mark,
            "unlock-check": cmd_unlock_check}[args.cmd](args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
