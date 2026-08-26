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

Fail-closed gate (v008 #18/#19): exits nonzero iff any REQUIRED dimension
(reference_ctest, unseen_grade, pipeline_grade when --pipeline,
grader_self_tests, foundations-c17 audit) is error, invalid, or an
unexpected not-run -> "overall": "red". Integration references still
"pending" downgrade green to "incomplete" WITHOUT failing the gate, so
local runs before the canonical binaries exist stay honest-but-passing.
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


def _verdict(required: dict[str, str],
             integration: dict[str, str]) -> tuple[str, bool]:
    """Return (overall, gate_ok) per the v008 #18/#19 contract.

    required  — dimension name -> status; anything but "green" is a
                missing/failed evidence and makes the run red.
    integration — ch51/ch52 -> pending|green|error; "pending" keeps the
                gate open (incomplete), only all-green earns green.
    """
    if any(st != "green" for st in required.values()):
        return "red", False
    if integration and all(v == "green" for v in integration.values()):
        return "green", True
    return "incomplete", True


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


def _ctest_state(returncode: int, stdout: str) -> dict:
    """Parse a ctest run into its evidence dimension (v008 blocker fix).

    Accepts BOTH summary formats:
      modern : "100% tests passed, 0 tests failed out of 414"
      legacy : "100% tests passed out of 414"
    Green requires returncode == 0 AND zero failed tests; an unparseable
    tail is an error, never a pass.
    """
    m = re.search(
        r"(\d+)% tests passed(?:,\s*(\d+) tests failed)? out of (\d+)",
        stdout)
    if not m:
        return {"status": "error", "reason": "ctest output unparseable",
                "ran": 0, "passed": 0, "failed": -1}
    total = int(m.group(3))
    failed = int(m.group(2)) if m.group(2) is not None else 0
    passed = total - max(failed, 0)
    if returncode != 0:
        # Keep the parseable summary so a failing run records its real
        # counts instead of sentinels (P2: rc!=0 discarded them).
        return {"status": "error", "reason": f"ctest exited {returncode}",
                "ran": total, "passed": passed, "failed": failed}
    status = "green" if failed == 0 else "error"
    return {"status": status, "ran": total, "passed": passed,
            "failed": failed}


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
        ctest_state = _ctest_state(proc.returncode, proc.stdout)
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
    required = {
        "reference_ctest": ctest_state["status"],
        "unseen_grade": unseen["status"],
        "grader_self_tests": self_state["status"],
        "foundations_c17_audit": ext["audit"],
    }
    if args.pipeline:
        required["pipeline_grade"] = pipeline["status"]

    def _ch52_status(repo: Path, build_dir: Path) -> str:
        runner = build_dir / "tools/labs/nes_gate/nes_gate_runner"
        golden = repo / "tests/public/ch52_nes_playable_gate/goldens/gate_reference.emu_gate"
        rom = repo / "tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes"
        if not runner.is_file() or not golden.is_file() or not rom.is_file():
            return "pending"
        try:
            cur = Path("/tmp/cur_gate_verify.emu_gate")
            subprocess.run(
                [str(runner), "--rom", str(rom), "--frames", "180",
                 "--gate", str(cur)], check=True, capture_output=True,
                 timeout=30)
            want = golden.read_text().strip().splitlines()
            got = cur.read_text().strip().splitlines()
            # Compare the pinned hashes (ROM/FRAME/AUDIO/PPU/RAM) — ignore REPLAY_FNV.
            def kv(lines):
                return {k: v for k, v in (l.split("=", 1) for l in lines if "=" in l)}
            a, b = kv(want), kv(got)
            for k in ("ROM_FNV", "FRAME_FNV", "AUDIO_FNV", "PPU_FNV", "RAM_FNV"):
                if a.get(k) != b.get(k):
                    return "error"
            return "green"
        except Exception:
            return "error"
    integration = {
        "ch52": _ch52_status(repo, ct),
        "ch51": "pending",
    }
    overall, gate_ok = _verdict(required, integration)

    current = {
        "generated_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "head": head,
        "platform": sys.platform,
        "overall": overall,
        "required": required,
        "reference_ctest": ctest_state,
        "unseen_grade": unseen,
        "pipeline_grade": pipeline,
        "grader_self_tests": self_state,
        "external_courses": {"foundations-c17": ext},
        "integration_reference": integration,
    }
    (repo / "verification-current.json").write_text(
        json.dumps(current, indent=2))
    print(json.dumps(current, indent=2))
    return 0 if gate_ok else 1


if __name__ == "__main__":
    sys.exit(main())