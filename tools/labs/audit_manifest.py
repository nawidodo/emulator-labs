#!/usr/bin/env python3
"""Manifest / DAG auditor for emulator-labs (delta review #14).

Validates the course infrastructure invariants that used to break silently:

  1. every requires[] entry points at a manifest chapter
  2. no dependency cycles (topological sort must cover all)
  3. every manifest chapter has a template dir (or is a declared virtual
     gate: capstone-style externally graded chapters list "virtual": true)
  4. no templates dir missing from the manifest
  5. optional chapters never appear inside the required chain of a
     non-optional chapter (optional branches stay optional)
  6. every chapter is reachable from some root
  7. verification.json has no entries for unknown chapters
  8. display_order values are unique positive integers

Exit 0 iff all invariants hold.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    mf_path = repo / "course-manifest.json"
    if not mf_path.is_file():
        print("[audit] course-manifest.json missing")
        return 1
    m = json.loads(mf_path.read_text())
    chapters: dict = m.get("chapters", {})
    problems: list[str] = []

    # 1. prerequisites exist
    for cid, e in chapters.items():
        for pre in e.get("requires", []):
            if pre not in chapters:
                problems.append(f"{cid}: unknown prerequisite '{pre}'")

    # 2. cycle detection via Kahn's algorithm
    indeg = {c: len(e.get("requires", [])) for c, e in chapters.items()}
    dependents = {c: [] for c in chapters}
    for cid, e in chapters.items():
        for pre in e.get("requires", []):
            if pre in dependents:
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
        problems.append(f"dependency cycle involving: {cyclic}")

    # 3. template exists or virtual
    for cid in chapters:
        if not (repo / "templates" / cid).is_dir() \
                and not chapters[cid].get("virtual", False):
            problems.append(f"{cid}: no templates/ dir and not virtual")

    # 4. template dirs missing from manifest
    for tdir in (repo / "templates").glob("ch*"):
        if tdir.name not in chapters and not tdir.name.startswith("ch00"):
            problems.append(f"{tdir.name}: template dir missing from manifest")

    # 5. optional leakage: a NON-optional chapter must not require an
    #    optional one (directly — transitive follows from direct edges).
    for cid, e in chapters.items():
        if e.get("optional"):
            continue
        for pre in e.get("requires", []):
            pre_e = chapters.get(pre, {})
            if pre_e.get("optional") and pre != cid:
                problems.append(f"{cid}: non-optional chapter requires "
                                f"optional '{pre}'")

    # 6. reachability from roots
    roots = [c for c, e in chapters.items() if not e.get("requires", [])]
    reach = set(roots)
    frontier = list(roots)
    while frontier:
        c = frontier.pop()
        for d in dependents.get(c, []):
            if d not in reach:
                reach.add(d)
                frontier.append(d)
    unreachable = sorted(set(chapters) - reach)
    if unreachable:
        problems.append(f"unreachable chapters: {unreachable}")

    # 7. verification.json sanity
    vj = repo / "verification.json"
    if vj.is_file():
        v = json.loads(vj.read_text())
        for cid in v.get("chapters", {}):
            if cid not in chapters:
                problems.append(f"verification.json: unknown chapter {cid}")

    # 8. display_order uniqueness/positivity
    orders = [e.get("display_order") for e in chapters.values()]
    if any(o is None or o <= 0 for o in orders):
        problems.append("display_order missing or non-positive somewhere")
    if len(set(orders)) != len(orders):
        dupes = sorted(o for o in orders if orders.count(o) > 1)
        problems.append(f"display_order duplicates: {dupes}")

    for pr in problems:
        print(f"[audit] PROBLEM: {pr}")
    print(f"[audit] {len(chapters)} chapters, {len(roots)} roots, "
          f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
