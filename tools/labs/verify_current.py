#!/usr/bin/env python3
"""Generate verification-current.json — the LIVE CI verification artifact
(v005 P1-11 / v006 #43-52).

Reads current build/ctest/grade/track state and writes a fresh snapshot
beside the certified verification.json. Distinguishes "not run" from
"ran and green" instead of silently reporting zero-state as complete.

Usage:
  python3 tools/labs/verify_current.py [--repo .] [--build-dir build-solutions]

Output schema (v006 #52):
  head, platform
  reference_ctest {status: green|not-run|error, ran, passed, failed}
  unseen_grade    {status: green|not-run|invalid, passed, skipped, failed, total}
  grader_self_tests {status: green|error, ran, failed}
  external_tracks {foundations-c17: {verified_nodes, audit, build}}
  integration_reference {ch52: pending|green|failed, ch51: ...}
"""

from __future__ import annotations

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--build-dir", default="build-solutions")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()

    head = subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                          cwd=repo, capture_output=True, text=True) \
        .stdout.strip()

    # ---- reference CTest: explicit not-run when no build dir/populated ----
    ct = repo / args.build_dir
    if (ct / "CTestTestfile.cmake").is_file():
        out = subprocess.run(["ctest", "--test-dir", str(ct)],
                             capture_output=True, text=True).stdout
        m = re.search(r"(\d+)% tests passed out of (\d+)", out)
        if m:
            percent, total = int(m.group(1)), int(m.group(2))
            failed = 0 if percent == 100 else -1
            passed = total - failed          # integer, no float rounding
            ctest_state = {"status": "green" if failed == 0 else "error",
                           "ran": total, "passed": passed, "failed": failed}
        else:
            ctest_state = {"status": "error", "reason":
                           "ctest output could not be parsed",
                           "ran": 0, "passed": 0, "failed": 0}
    else:
        ctest_state = {"status": "not-run",
                       "reason": f"{args.build_dir}/CTestTestfile.cmake "
                                 "missing",
                       "ran": 0, "passed": 0, "failed": 0}

    # ---- unseen grade sweep (author mode: skip env-gated) ----
    grade_out = subprocess.run(
        [sys.executable, "tools/labs/grade.py", "--allow-missing-env",
         "--repo", str(repo)],
        capture_output=True, text=True).stdout
    g = re.search(
        r"(\d+) passed / (\d+) skipped / (\d+) failed \(of (\d+)\)",
        grade_out)
    if g:
        passed, skipped, failed, total = (int(g.group(1)), int(g.group(2)),
                                          int(g.group(3)), int(g.group(4)))
        grade_state = {"status": "green" if failed == 0 else "error",
                       "passed": passed, "skipped": skipped,
                       "failed": failed, "total": total}
    else:
        grade_state = {"status": "invalid", "passed": -1, "skipped": -1,
                       "failed": -1, "total": -1,
                       "reason": "grade summary could not be parsed"}

    # ---- grader self-tests (v006 #51) ----
    gt = subprocess.run(
        [sys.executable, "-m", "unittest", "discover", "-s",
         "tools/labs/tests", "-v"],
        cwd=repo, capture_output=True, text=True)
    gm = re.search(r"Ran (\d+) tests", gt.stdout + gt.stderr)
    self_state = {"status": "green" if gt.returncode == 0 else "error",
                  "ran": int(gm.group(1)) if gm else 0,
                  "failed": 0 if gt.returncode == 0 else -1}

    # ---- external C17 track: derived verified count + audit (v006 #49) ----
    c17 = {}
    tmf = repo / "tracks" / "foundations-c17" / "manifest.json"
    if tmf.is_file():
        m = json.loads(tmf.read_text())
        verified = sum(1 for e in m.get("chapters", {}).values()
                       if e.get("implementation") == "verified")
        audit = subprocess.run(
            [sys.executable, "tools/labs/audit_track.py", "foundations-c17"],
            cwd=repo, capture_output=True, text=True)
        c17 = {"verified_nodes": verified,
               "audit": "green" if audit.returncode == 0 else "error"}
    else:
        c17 = {"verified_nodes": 0, "audit": "invalid",
               "reason": "track manifest missing"}

    current = {
        "generated_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "head": head,
        "platform": sys.platform,
        "reference_ctest": ctest_state,
        "unseen_grade": grade_state,
        "grader_self_tests": self_state,
        "external_tracks": {"foundations-c17": c17},
        "integration_reference": {
            "ch52": "pending",   # derived once canonical reference exists
            "ch51": "pending",
        },
    }
    (repo / "verification-current.json").write_text(
        json.dumps(current, indent=2))
    print(json.dumps(current, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())