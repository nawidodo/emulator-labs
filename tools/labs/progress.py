#!/usr/bin/env python3
"""Chapter-gate progress tracker for emulator-labs (curriculum §4, §60).

Unlock model (review recommendation #2): chapters unlock when their EXPLICIT
PREREQUISITES pass, not merely because the previous chapter finished.

Prerequisite sources, in priority order:
  1. course-manifest.json   -> {"chapters": {id: {"requires": [...]}}}
  2. progress.json legacy   -> implicit "previous chapter in list" chain

Learner state lives in .emulator-labs/progress.json (gitignored) so several
learners can use one clone; author verification lives in the repo.

Usage:
  progress.py status
  progress.py mark <chapter> <component> passed|failed|active
  progress.py unlock-check <chapter>
Components: exercises starter debug challenge coding_test
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

COMPONENTS = ["exercises", "starter", "debug", "challenge", "coding_test"]
GATE_MARK = "passed"
LEARNER_DIR = ".emulator-labs"


def repo() -> Path:
    return Path(__file__).resolve().parents[2]


def learner_progress_path() -> Path:
    return repo() / LEARNER_DIR / "progress.json"


def load() -> dict:
    """Load learner progress, migrating the legacy repo-root location once."""
    p = learner_progress_path()
    if p.is_file():
        return json.loads(p.read_text())
    legacy = repo() / "progress.json"
    if legacy.is_file():
        data = json.loads(legacy.read_text())
        save(data)
        return data
    sys.exit("error: no progress file found")


def save(data: dict) -> None:
    p = learner_progress_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=2) + "\n")


def requires_for(ch: str) -> list[str]:
    """Explicit prerequisites from course-manifest.json, else linear chain."""
    mf = repo() / "course-manifest.json"
    if mf.is_file():
        entry = json.loads(mf.read_text()).get("chapters", {}).get(ch)
        if entry is not None:
            return list(entry.get("requires", []))
    order = list(json.loads((repo() / "progress.json").read_text())
                 ["chapters"].keys()) \
        if (repo() / "progress.json").is_file() else []
    if ch in order:
        i = order.index(ch)
        return order[:i]           # every earlier chapter is a prerequisite
    return []


def chapters_in_order(data: dict) -> list[str]:
    return sorted(data["chapters"].keys())


def gate_passed(st: dict) -> bool:
    return all(st["components"][c] == GATE_MARK for c in COMPONENTS)


def is_unlocked(data: dict, ch: str) -> bool:
    if ch not in data["chapters"]:
        return False
    # A chapter unlocks when ALL of its prerequisites have full gates.
    for pre in requires_for(ch):
        pre_state = data["chapters"].get(pre)
        if pre_state is None or not gate_passed(pre_state):
            return False
    # No prerequisites declared -> first chapter(s) are open by definition.
    return True


def cmd_status(_: argparse.Namespace) -> int:
    data = load()
    unlocked = [c for c in chapters_in_order(data) if is_unlocked(data, c)]
    cur = data["student"]["current"]
    print("# Emulator School — gated laboratory curriculum\n")
    print(f"Current Chapter: {cur}")
    print(f"Unlocked Chapters: {len(unlocked)}\n")
    hdr = (f"| {'Chapter':<38} | "
           + " | ".join(f"{c[:9]:>9}" for c in COMPONENTS) + " | Status  |")
    print(hdr)
    print("|" + "-" * (len(hdr) - 2) + "|")
    for ch in chapters_in_order(data):
        st = data["chapters"][ch]
        cells = [({"passed": "     ✓   ", "failed": "   ✗    ",
                   "active": "  ~     "}.get(st["components"][c], "    -    "))
                 for c in COMPONENTS]
        all_pass = gate_passed(st)
        state = ("PASSED" if all_pass else
                 ("ACTIVE" if ch == cur else
                  ("unlocked" if is_unlocked(data, ch) else "🔒 LOCKED")))
        print(f"| {ch:<38} | " + " | ".join(cells) + f" | {state} |")
    return 0


def cmd_mark(ns: argparse.Namespace) -> int:
    data = load()
    ch = ns.chapter
    if ch not in data["chapters"]:
        sys.exit(f"error: unknown chapter '{ch}'")
    if not is_unlocked(data, ch):
        sys.exit(f"error: {ch} is LOCKED — prerequisites not passed yet")
    data["chapters"][ch]["components"][ns.component] = ns.state

    comps = data["chapters"][ch]["components"]
    if all(comps[c] == GATE_MARK for c in COMPONENTS):
        nxt = [c for c in chapters_in_order(data)
               if not gate_passed(data["chapters"][c])
               and is_unlocked(data, c)]
        if nxt:
            data["student"]["current"] = nxt[0]
            print(f"GATE PASSED: {ch} -> {nxt[0]} ACTIVE")
        else:
            print(f"GATE PASSED: {ch} (no further open chapter)")
    save(data)
    print(f"marked {ch}/{ns.component} = {ns.state}")
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
    try:
        return {"status": cmd_status, "mark": cmd_mark,
                "unlock-check": cmd_unlock_check}[args.cmd](args)
    except BrokenPipeError:
        return 0


if __name__ == "__main__":
    sys.exit(main())
