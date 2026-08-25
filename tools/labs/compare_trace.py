#!/usr/bin/env python3
"""Trace differ for emulator-labs (trace-first debugging, curriculum §54).

Canonical trace line format produced by every chapter's headless runner:

    pc=0200 op=00E0 V0=00 I=000 SP=0F cyc=11

Fields are whitespace-separated key=value tokens. The comparer aligns lines
by index, reports the FIRST divergence with context, and can ignore selected
fields (typically cyc when comparing against a reference emulator with
different cycle accounting granularity).

Usage:
  compare_trace.py expected.log actual.log [--ignore cyc,V0] [--context 3]
  compare_trace.py --manifest traces/trace-manifest.json   # batch mode
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def parse_trace(text: str) -> list[dict[str, str]]:
    rows = []
    for lineno, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        row = {}
        for tok in line.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                row[k.lower()] = v.lower()
        if row:
            row["_line"] = lineno
            rows.append(row)
    return rows


def first_divergence(exp: list[dict], act: list[dict],
                     ignore: set[str]) -> str | None:
    for i in range(max(len(exp), len(act))):
        if i >= len(exp):
            return f"line {i + 1}: actual trace has extra instruction(s) " \
                   f"({len(act)} vs {len(exp)} lines)"
        if i >= len(act):
            return f"line {i + 1}: actual trace ended early " \
                   f"({len(act)} vs {len(exp)} lines)"
        e, a = exp[i], act[i]
        keys = (e.keys() | a.keys()) - ignore - {"_line"}
        diff = [k for k in sorted(keys) if e.get(k) != a.get(k)]
        if diff:
            detail = ", ".join(f"{k}: expected {e.get(k, '?')!r} "
                               f"got {a.get(k, '?')!r}" for k in diff)
            return f"line {i + 1} (first divergence): {detail}"
    return None


def compare_files(expected: Path, actual: Path, ignore: set[str],
                  context: int) -> int:
    exp = parse_trace(expected.read_text(errors="replace"))
    act = parse_trace(actual.read_text(errors="replace"))
    problem = first_divergence(exp, act, ignore)
    if problem is None:
        print(f"[TRACE OK] {actual.name} == {expected.name} "
              f"({len(act)} instructions)")
        return 0
    print(f"[TRACE FAIL] {actual}")
    print(f"  {problem}")
    idx = int(problem.split()[1].split(":")[0]) - 1
    lo, hi = max(0, idx - context), min(len(act), idx + context + 1)
    print("  context (actual):")
    for r in act[lo:hi]:
        marker = ">>" if r["_line"] == idx + 1 else "  "
        fields = " ".join(f"{k}={v}" for k, v in sorted(r.items())
                          if k != "_line")
        print(f"    {marker} {r['_line']:>6}: {fields}")
    return 1


def run_manifest(manifest: Path) -> int:
    spec = json.loads(manifest.read_text())
    base = manifest.parent.parent
    bad = 0
    for case in spec["cases"]:
        ign = set(case.get("ignore", []))
        rc = compare_files(base / case["expected"], base / case["actual"],
                           ign, case.get("context", 3))
        if rc and case.get("optional"):
            print("  (optional case, not gating)")
            bad += 0
        else:
            bad |= rc
    return 1 if bad else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("expected", nargs="?", type=Path)
    ap.add_argument("actual", nargs="?", type=Path)
    ap.add_argument("--ignore", default="", help="comma-separated fields")
    ap.add_argument("--context", type=int, default=3)
    ap.add_argument("--manifest", type=Path,
                    help="batch mode: JSON with cases[]")
    args = ap.parse_args()
    if args.manifest:
        return run_manifest(args.manifest)
    if not args.expected or not args.actual:
        ap.error("need expected.log actual.log or --manifest")
    return compare_files(args.expected, args.actual,
                         {f.strip().lower() for f in args.ignore.split(",") if f},
                         args.context)


if __name__ == "__main__":
    sys.exit(main())
