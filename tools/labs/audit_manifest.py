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


GATE_COMPONENTS = ["boot", "input", "video", "audio",
                  "determinism", "integration_test"]
DEFAULT_COMPONENTS = ["exercises", "starter", "debug", "challenge",
                      "coding_test"]


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

    # 8b. Schema v3 checks (comprehensive review #12)
    declared_tracks = set(m.get("tracks", []))
    for cid, e in chapters.items():
        for t in e.get("track", []):
            if t not in declared_tracks:
                problems.append(f"{cid}: track '{t}' not declared in "
                                f"manifest.tracks")
        gf = e.get("gate_for")
        if gf and gf not in declared_tracks:
            problems.append(f"{cid}: gate_for '{gf}' not a declared track")
        kind = e.get("kind", "lab")
        if kind not in ("lab", "integration_gate", "capstone"):
            problems.append(f"{cid}: unknown kind '{kind}'")
        comps = e.get("components")
        if kind == "integration_gate" and comps != GATE_COMPONENTS:
            problems.append(f"{cid}: integration_gate must use its gate "
                            f"components")
    used_tracks = {t for e in chapters.values() for t in e.get("track", [])}
    meta = m.get("tracks_meta", {})
    for t in declared_tracks:
        if t not in used_tracks and t not in meta:
            print(f"[audit] WARNING: declared track '{t}' has no chapters "
                  f"and no tracks_meta entry")

    # 8c. kind-artifact contract (review #12.4): integration gates need
    #     an integration acceptance manifest; labs need a hidden manifest.
    for cid, e in chapters.items():
        kind = e.get("kind", "lab")
        hdir = repo / "tests" / "hidden" / cid
        if kind == "integration_gate":
            if not (hdir / "manifest.json").is_file():
                problems.append(f"{cid}: integration_gate missing hidden "
                                f"manifest")
        elif kind == "lab":
            if not (hdir / "manifest.json").is_file():
                problems.append(f"{cid}: lab missing hidden manifest")

    # 9. Redundant-edge WARNINGS (fresh-review #14): an edge A->B is
    #    redundant when B stays reachable from A's other prerequisites.
    #    Warning-only: the graph may carry these while being simplified.
    def ancestors(chid):
        out = set()
        stack = list(chapters.get(chid, {}).get("requires", []))
        while stack:
            x = stack.pop()
            if x in out or x not in chapters:
                continue
            out.add(x)
            stack.extend(chapters[x].get("requires", []))
        return out

    for cid, e in chapters.items():
        reqs = list(e.get("requires", []))
        for pre in reqs:
            others = [x for x in reqs if x != pre]
            if any(pre == other or pre in ancestors(other) for other in others):
                print(f"[audit] WARNING: redundant edge {cid} -> {pre} "
                      f"(already reachable through another prerequisite)")

    # 10. Track-reachability policy tests (fresh-review #15).
    def reachable_from(target, roots):
        seen_set, frontier = set(roots), list(roots)
        while frontier:
            c = frontier.pop()
            for d in dependents.get(c, []):
                if d not in seen_set:
                    seen_set.add(d)
                    frontier.append(d)
        return target in seen_set

    core_roots = [c for c, e in chapters.items()
                  if not e.get("requires", [])]
    ps1_cap = "ch51_ps1_capstone"
    gate = "ch52_nes_playable_gate"
    core_tracks = {"core", "nes-depth", "ps1"}
    # Allowed-track subgraph: chapters whose track intersects core_tracks
    # (classic-depth-only chapters are invisible to the ps1 route).
    allowed = {c for c, e in chapters.items()
               if set(e.get("track", [])) & core_tracks}
    core_deps = {c: [r for r in e.get("requires", []) if r in allowed]
                 for c, e in chapters.items() if c in allowed}
    frontier = [c for c in allowed if not core_deps.get(c, [])]
    seen_set = set(frontier)
    while frontier:
        c = frontier.pop()
        for d, reqs in core_deps.items():
            if c in reqs and d not in seen_set:
                seen_set.add(d)
                frontier.append(d)
    if ps1_cap not in seen_set:
        problems.append(f"policy: {ps1_cap} unreachable through "
                        f"core/nes-depth/ps1 tracks alone")
    if gate not in seen_set:
        problems.append(f"policy: {gate} unreachable through the core route")

    # Classic-depth route remains internally reachable when selected.
    classic = {c for c, e in chapters.items()
               if "classic-depth" in e.get("track", [])}
    c_roots = [c for c in classic
               if all(r not in classic for r in chapters[c]
                      .get("requires", []))]
    c_seen, c_frontier = set(c_roots), list(c_roots)
    while c_frontier:
        c = c_frontier.pop()
        for d in classic:
            if c in chapters[d].get("requires", []) and d not in c_seen:
                c_seen.add(d)
                c_frontier.append(d)
    if len(c_seen) != len(classic):
        problems.append("policy: classic-depth branch not fully reachable")

    for pr in problems:
        print(f"[audit] PROBLEM: {pr}")
    print(f"[audit] {len(chapters)} chapters, {len(roots)} roots, "
          f"{len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
