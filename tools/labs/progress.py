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

def components_for(ch: str) -> list[str]:
    """Gate components for one chapter: manifest `components` wins over
    the default five (integration gates/capstones differ)."""
    e = manifest_entry(ch)
    return list(e.get("components", COMPONENTS)) or COMPONENTS
LEARNER_DIR = ".emulator-labs"


def repo() -> Path:
    return Path(__file__).resolve().parents[2]


def learner_progress_path() -> Path:
    return repo() / LEARNER_DIR / "progress.json"


def initial_state_from_manifest() -> dict:
    """Fresh-clone initialization: build learner state from the manifest
    (delta review #13). Every chapter starts LOCKED with empty gates."""
    mf = repo() / "course-manifest.json"
    data = {"student": {"current": "", "track": ""}, "chapters": {}}
    if mf.is_file():
        mch = json.loads(mf.read_text()).get("chapters", {})
        for cid, e in mch.items():
            data["chapters"][cid] = {
                "title": e.get("title", cid),
                "components": {c: "" for c in components_for(cid)},
                "status": "LOCKED",
            }
    open_chapters = [c for c in chapters_in_order(data)
                     if is_unlocked(data, c)]
    data["student"]["current"] = (open_chapters[0]
                                  if open_chapters else "")
    return data


def load() -> dict:
    """Load learner progress, initializing/migrating as needed."""
    p = learner_progress_path()
    legacy = repo() / "progress.json"
    if p.is_file():
        data = json.loads(p.read_text())
    elif legacy.is_file():
        data = json.loads(legacy.read_text())
        save(data)                      # one-time migration
    else:
        data = initial_state_from_manifest()
        save(data)

    # Merge-on-read: new manifest chapters appear automatically, removed
    # ones disappear, titles refresh.
    mf = repo() / "course-manifest.json"
    if mf.is_file():
        mch = json.loads(mf.read_text()).get("chapters", {})
        for cid, e in mch.items():
            if cid not in data["chapters"]:
                data["chapters"][cid] = {
                    "title": e.get("title", cid),
                    "components": {c: "" for c in components_for(cid)},
                    "status": "LOCKED"}
            else:
                data["chapters"][cid]["title"] = e.get("title", cid)
                # merge newly-introduced manifest components into state
                for c in components_for(cid):
                    data["chapters"][cid]["components"].setdefault(c, "")
        for cid in list(data["chapters"]):
            if cid not in mch:
                del data["chapters"][cid]
        save(data)
    return data


def save(data: dict) -> None:
    p = learner_progress_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=2) + "\n")


def requires_for(ch: str) -> list[str]:
    """DIRECT prerequisites from course-manifest.json (review delta #3).
    Falls back to a linear chain only when no manifest exists."""
    mf = repo() / "course-manifest.json"
    if mf.is_file():
        entry = json.loads(mf.read_text()).get("chapters", {}).get(ch)
        if entry is not None:
            return list(entry.get("requires", []))
    order = []
    pf = learner_progress_path()
    if pf.is_file():
        order = list(json.loads(pf.read_text())["chapters"].keys())
    elif (repo() / "progress.json").is_file():
        order = list(json.loads((repo() / "progress.json")
                       .read_text())["chapters"].keys())
    if ch in order:
        i = order.index(ch)
        return order[:i]
    return []


def chapters_in_order(data: dict) -> list[str]:
    """Presentation order: manifest display_order (fresh-review #8),
    falling back to lexicographic ids."""
    disp = manifest_display_order()
    return sorted(data["chapters"].keys(),
                  key=lambda c: (disp.get(c, 10_000), c))


def manifest_display_order() -> dict:
    mf = repo() / "course-manifest.json"
    if not mf.is_file():
        return {}
    mch = json.loads(mf.read_text()).get("chapters", {})
    return {cid: e.get("display_order", 10_000 + i)
            for i, (cid, e) in enumerate(sorted(mch.items()))}


def gate_passed(st: dict) -> bool:
    return all(v == GATE_MARK for v in st["components"].values())


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
        nxt = next_open_chapter(data)
        if nxt:
            data["student"]["current"] = nxt
            print(f"GATE PASSED: {ch} -> {nxt} ACTIVE")
        else:
            print(f"GATE PASSED: {ch} (course complete!)")
    save(data)
    print(f"marked {ch}/{ns.component} = {ns.state}")
    return 0


def manifest_entry(ch: str) -> dict:
    mf = repo() / "course-manifest.json"
    if mf.is_file():
        return json.loads(mf.read_text()).get("chapters", {}).get(ch, {})
    return {}


def selected_tracks(data: dict) -> set[str]:
    st = (data.get("student", {}) or {})
    sel = st.get("selected_tracks") or ([st["track"]] if st.get("track")
                                        else [])
    return set(sel)


def valid_tracks() -> list[str]:
    mf = repo() / "course-manifest.json"
    if mf.is_file():
        return json.loads(mf.read_text()).get("tracks", [])
    return []


def cmd_why(ns: argparse.Namespace) -> int:
    data = load()
    ch = ns.chapter
    if ch not in data["chapters"]:
        sys.exit(f"error: unknown chapter '{ch}'")
    if is_unlocked(data, ch):
        print(f"{ch}: UNLOCKED")
        return 0
    print(f"{ch}: LOCKED")
    for pre in requires_for(ch):
        st = data["chapters"].get(pre)
        if st is None:
            print(f"  - {pre}: (no learner state)")
        elif gate_passed(st):
            print(f"  - {pre}: PASSED")
        else:
            missing = [c for c, v in st["components"].items() if v != "passed"]
            print(f"  - {pre}: NOT PASSED (pending: {', '.join(missing)})")
    return 2


def cmd_tracks(ns: argparse.Namespace) -> int:
    data = load()
    mf = repo() / "course-manifest.json"
    valid = json.loads(mf.read_text()).get("tracks", []) if mf.is_file() \
        else []
    sel = selected_tracks(data)
    print("tracks:", ", ".join(valid))
    print("selected:", ", ".join(sorted(sel)) or "(none — full course)")
    return 0


def cmd_track_add(ns: argparse.Namespace) -> int:
    data = load()
    valid = valid_tracks()
    if ns.track_name not in valid:
        sys.exit(f"error: unknown track '{ns.track_name}' (valid: {valid})")
    st = data.setdefault("student", {})
    sel = set(st.get("selected_tracks", []))
    sel.add(ns.track_name)
    st["selected_tracks"] = sorted(sel)
    save(data)
    print("selected tracks:", ", ".join(sorted(sel)))
    nxt = next_open_chapter(data)
    print("next open chapter:", nxt or "(none)")
    return 0


def cmd_track_remove(ns: argparse.Namespace) -> int:
    data = load()
    st = data.setdefault("student", {})
    sel = set(st.get("selected_tracks", []))
    sel.discard(ns.track_name)
    st["selected_tracks"] = sorted(sel)
    save(data)
    print("selected tracks:", ", ".join(sorted(sel)) or "(none)")
    nxt = next_open_chapter(data)
    print("next open chapter:", nxt or "(none)")
    return 0


def next_open_chapter(data: dict) -> str:
    """Earliest unlocked, unfinished chapter. With an active track set,
    optional chapters outside that track are deferred to the end."""
    sel = selected_tracks(data)
    on_track, off_track = [], []
    for c in chapters_in_order(data):
        st = data["chapters"][c]
        if gate_passed(st) or not is_unlocked(data, c):
            continue
        mf_entry = manifest_entry(c)
        optional = mf_entry.get("optional", False)
        tracks = set(mf_entry.get("track", []))
        if sel and optional and not (sel & tracks):
            off_track.append(c)
        else:
            on_track.append(c)
    return (on_track + off_track)[0] if (on_track or off_track) else ""


def cmd_track(ns: argparse.Namespace) -> int:
    """Compatibility wrapper: 'track X' == select track X alone."""
    if ns.track_name and ns.track_name not in valid_tracks():
        sys.exit(f"error: unknown track '{ns.track_name}' "
                 f"(valid: {valid_tracks()})")
    data = load()
    st = data.setdefault("student", {})
    st["selected_tracks"] = [ns.track_name] if ns.track_name else []
    save(data)
    print(f"selected tracks: {ns.track_name or '(none — full course)'}")
    nxt = next_open_chapter(data)
    print(f"next open chapter: {nxt or '(none)'}")
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
    w = sub.add_parser("why")
    w.add_argument("chapter")
    t = sub.add_parser("track")
    t.add_argument("track_name", nargs="?", default="",
                   help="select one track (alias for track-add)")
    sub.add_parser("tracks")
    ta = sub.add_parser("track-add")
    ta.add_argument("track_name")
    tr = sub.add_parser("track-remove")
    tr.add_argument("track_name")
    args = ap.parse_args()
    try:
        table = {"status": cmd_status, "mark": cmd_mark,
                 "track": cmd_track, "tracks": cmd_tracks,
                 "track-add": cmd_track_add,
                 "track-remove": cmd_track_remove,
                 "unlock-check": cmd_unlock_check,
                 "why": cmd_why}
        return table[args.cmd](args)
    except BrokenPipeError:
        return 0


if __name__ == "__main__":
    sys.exit(main())
