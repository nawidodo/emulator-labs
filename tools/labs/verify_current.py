#!/usr/bin/env python3
"""Generate verification-current.json (live CI artifact, v005 P1-11).

Reads current build/ctest/grade state and writes a fresh snapshot beside
the certified verification.json. Designed to run at the end of CI.

Usage: python3 tools/labs/verify_current.py [--repo .] [--build-dir build-solutions]
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--build-dir", default="build-solutions")
    ap.add_argument("--pipeline", action="store_true",
                    help="also grade pipeline self-checks")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()

    head = subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                          cwd=repo, capture_output=True, text=True) \
        .stdout.strip()

    # ctest counts from the reference build dir
    ctest = {"ran": 0, "passed": 0, "failed": 0}
    ct = repo / args.build_dir
    if (ct / "CTestTestfile.cmake").is_file():
        out = subprocess.run(["ctest", "--test-dir", str(ct)],
                             capture_output=True, text=True).stdout
        import re
        m = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)",
                      out)
        if m:
            ctest = {"passed": int(m.group(1)) / 100 * int(m.group(3)),
                     "failed": int(m.group(2)), "ran": int(m.group(3))}

    # unseen grade sweep (author mode: skip env-gated)
    grade_out = subprocess.run(
        [sys.executable, "tools/labs/grade.py", "--allow-missing-env",
         "--repo", str(repo)],
        capture_output=True, text=True).stdout
    import re as _re
    g = _re.search(r"(\d+) passed / (\d+) skipped / (\d+) failed \(of (\d+)\)",
                   grade_out)
    grade_state = {"passed": g.group(1), "skipped": g.group(2),
                   "failed": g.group(3), "of": g.group(4)} if g \
        else {"passed": "?", "skipped": "?", "failed": "?", "of": "?"}

    current = {
        "generated_at": __import__("datetime")
            .datetime.now().isoformat(timespec="seconds"),
        "head": head,
        "ctest": ctest,
        "unseen_grade": grade_state,
        "integration_reference": {
            "ch52": "pending",
            "ch51": "pending",
        },
        "c17_verified_nodes": 1,
        "platform": sys.platform,
    }
    (repo / "verification-current.json").write_text(
        json.dumps(current, indent=2))
    print(json.dumps(current, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
