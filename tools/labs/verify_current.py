#!/usr/bin/env python3
"""Generate verification-current.json — the LIVE CI verification artifact
(v005 P1-11 / v007 #33-45).

Rules (governing principle: "missing evidence is not a passing result"):

  reference_ctest  : not-run|green|error — never zero-green-looking
  unseen_grade     : green|error|invalid — integers, returncode checked
  pipeline_grade   : green|error|invalid — honors --pipeline (v007 #39)
  grader_self_tests: green|error with ran/failed from unittest
  external_courses : foundations-c17 audit + DERIVED verified count
  integration_reference: ch52/ch51 pending|green|error (derived once a
                    canonical reference binary is made available)

Usage:
  python3 tools/labs/verify_current.py [--repo .] [--build-dir build-solutions]
                                       [--pipeline]
"""

from __future__ import annotations

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path


def _run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def _grade_status(repo: Path, argv: list[str]) -> dict:
    proc = _run([sys.executable, "tools/labs/grade.py", *argv,
                 "--repo", str(repo)], repo)
    m = re.search(
        r"(\d+) passed / (\d+) skipped / (\d+) failed \(of (\d+)\)",
        proc.stdout)
    if m:
        passed, skipped, failed, total = (int(m.group(1)), int(m.group(2)),
                                          int(m.group(3)), int(m.group(4)))
        return {"status": "green" if proc.returncode == 0 and failed == 0
                else ("error" if failed > 0 else "invalid"),
                "passed": passed, "skipped": skipped, "failed": failed,
                "total": total}
    return {"status": "invalid", "passed": -1, "skipped": -1,
            "failed": -1, "total": -1,
            "reason": "grade summary could not be parsed"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--build-dir", default="build-solutions")
    ap.add_argument("--pipeline", action="store_true",
                    help="also grade tests/pipeline manifests")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()

    head = _run(["git", "rev-parse", "--short", "HEAD"], repo).stdout.strip()

    # ---- reference CTest: returncode-first, explicit not-run ----
    ct = repo / args.build_dir
    if (ct / "CTestTestfile.cmake").is_file():
        proc = _run(["ctest", "--test-dir", str(ct), "--output-on-failure"],
                    repo)
        m = re.search(r"(\d+)% tests passed out of (\d+)", proc.stdout)
        if proc.returncode != 0 or not m:
            ctest_state = {"status": "error",
                           "reason": proc.returncode != 0
                           and f"ctest exited {proc.returncode}"
                           or "ctest output unparseable",
                           "ran": 0, "passed": 0, "failed": -1}
        else:
            total = int(m.group(2))
            failed = 0 if proc.returncode == 0 else -1
            ctest_state = {"status": "green", "ran": total,
                           "passed": total - max(failed, 0),
                           "failed": failed}
    else:
        ctest_state = {"status": "not-run",
                       "reason": f"{args.build_dir}/CTestTestfile.cmake "
                                 "missing",
                       "ran": 0, "passed": 0, "failed": 0}

    # ---- unseen grade sweep ----
    unseen = _grade_status(repo, ["--allow-missing-env"])

    # ---- pipeline grade sweep (v007 #39: the flag now does something) ----
    pipeline = {"status": "skipped"}
    if args.pipeline:
        proc = _run([sys.executable, "tools/labs/grade.py", "--pipeline",
                     "--allow-missing-env", "--repo", str(repo)], repo)
        m = re.search(r"(\d+) passed / (\d+) skipped / (\d+) failed "
                      r"\(of (\d+)\)", proc.stdout)
        if m and proc.returncode == 0 and int(m.group(3)) == 0:
            pipeline = {"status": "green", "passed": int(m.group(1)),
                        "skipped": int(m.group(2)), "failed": int(m.group(3))}
        else:
            pipeline = {"status": "error" if proc.returncode != 0
                        else "invalid", "reason": "pipeline sweep failed"}

    # ---- grader self-tests ----
    gt = _run([sys.executable, "-m", "unittest", "discover", "-s",
               "tools/labs/tests", "-v"], repo)
    gm = re.search(r"Ran (\d+) tests", gt.stdout + gt.stderr)
    self_state = {"status": "green" if gt.returncode == 0 else "error",
                  "ran": int(gm.group(1)) if gm else 0,
                  "failed": 0 if gt.returncode == 0 else -1}

    # ---- external C17 track: derived count + audit ----
    tmf = repo / "tracks" / "foundations-c17" / "manifest.json"
    if tmf.is_file():
        m = json.loads(tmf.read_text())
        verified = sum(1 for e in m.get("chapters", {}).values()
                       if e.get("implementation") == "verified")
        audit = _run([sys.executable, "tools/labs/audit_track.py",
                      "foundations-c17"], repo)
        ext = {"audit": "green" if audit.returncode == 0 else "error",
               "verified_nodes": verified}
    else:
        ext = {"audit": "invalid", "verified_nodes": -1,
               "reason": "track manifest missing"}

    current = {
        "generated_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "head": head,
        "platform": sys.platform,
        "reference_ctest": ctest_state,
        "unseen_grade": unseen,
        "pipeline_grade": pipeline,
        "grader_self_tests": self_state,
        "external_courses": {"foundations-c17": ext},
        "integration_reference": {
            "ch52": "pending",   # derived once canonical reference runs
            "ch51": "pending",
        },
    }
    (repo / "verification-current.json").write_text(
        json.dumps(current, indent=2))
    print(json.dumps(current, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())