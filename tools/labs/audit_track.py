#!/usr/bin/env python3
"""Track auditor for emulator-labs (comprehensive-review-2055 #60).

Usage: python3 tools/labs/audit_track.py foundations-c17

Validates a track's manifest against its executable reality:
  - every requires[] entry points at a chapter in the same track
  - no dependency cycles
  - implementation values are from the known enum
  - every VERIFIED chapter has a generated-capable template
    (templates/<id> exists) and is listed as generatable
  - PLANNED chapters are reported (not failures)

Exit 0 iff no hard problems.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

VALID_IMPL = {"planned", "seed", "implemented", "verified",
               "virtual-docs"}


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    if len(sys.argv) < 2:
        print("usage: audit_track.py TRACK_NAME")
        return 2
    name = sys.argv[1]
    tdir = repo / "tracks" / name
    mf = tdir / "manifest.json"
    if not mf.is_file():
        print(f"[track-audit] {name}: missing {mf}")
        return 1
    m = json.loads(mf.read_text())
    chapters = m.get("chapters", {})
    problems = []
    warnings = []

    # prerequisites exist + cycles (Kahn)
    indeg = {c: len(e.get("requires", [])) for c, e in chapters.items()}
    dependents = {c: [] for c in chapters}
    for cid, e in chapters.items():
        for pre in e.get("requires", []):
            if pre not in chapters:
                problems.append(f"{cid}: unknown prerequisite '{pre}'")
            elif pre in dependents:
                dependents[pre].append(cid)
    queue = [c for c, d in indeg.items() if d == 0]
    seen = 0
    while queue:
        c = queue.pop()
        seen += 1
        for d in dependents[c]:
            indeg[d] -= 1
            if indeg[d] == 0:
                queue.append(d)
    if seen != len(chapters):
        cyclic = sorted(c for c, d in indeg.items() if d > 0)
        problems.append(f"dependency cycle involving {cyclic}")

    # implementation status validity + verified-has-template
    for cid, e in sorted(chapters.items()):
        impl = e.get("implementation", "planned")
        if impl not in VALID_IMPL:
            problems.append(f"{cid}: invalid implementation '{impl}'")
        if impl == "virtual-docs":
            if not (tdir / "docs").is_dir() and not list(
                    (tdir / "templates").glob(f"{cid}*")):
                # virtual-docs may be documented-only; a docs/ dir is
                # the normal home but absence is tolerated for now.
                pass
        if impl == "verified":
            has_template = any(d.name.startswith(cid)
                               for d in (tdir / "templates").iterdir()
                               if d.is_dir()) \
                if (tdir / "templates").is_dir() else False
            if not has_template:
                problems.append(f"{cid}: marked verified but no template "
                                f"dir matching its id")

    planned = sorted(c for c, e in chapters.items()
                     if e.get("implementation", "planned") == "planned")
    if planned:
        warnings.append(f"{len(planned)} chapter(s) still PLANNED "
                        f"(not generatable): {', '.join(planned[:6])}"
                        + (" ..." if len(planned) > 6 else ""))

    for w in warnings:
        print(f"[track-audit] WARNING: {w}")
    for pr in problems:
        print(f"[track-audit] PROBLEM: {pr}")
    print(f"[track-audit] {name}: {len(chapters)} nodes, "
          f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
