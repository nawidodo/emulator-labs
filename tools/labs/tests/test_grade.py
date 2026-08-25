#!/usr/bin/env python3
"""Direct grader self-tests (v005 §23 / v006 §24-27).

Covers the negative paths that normal success-only sweeps never touch:

    required_env absent -> FAIL
    --allow-missing-env -> SKIP
    missing binary      -> FAIL (+ artifact)
    optional ROM absent -> SKIP
    timeout             -> FAIL (+ stdout/stderr artifacts)
    crash/signal        -> FAIL
    unexpected exit     -> FAIL
    stdout mismatch     -> FAIL
    expected file missing -> FAIL
    hash mismatch       -> FAIL (+ produced artifact PRESERVED and asserted)
    successful hash     -> PASS
    grade-last semantics   (end-to-end main() sweep serializes
                            PASS / SKIP / FAIL distinctly)

Run with:  python3 -m unittest discover -s tools/labs/tests -v
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "labs"))

import grade

FIX = Path(__file__).parent / "fixtures"


class GraderNegatives(unittest.TestCase):
    def setUp(self):
        grade.ALLOW_MISSING_ENV = False
        self.repo = Path(tempfile.mkdtemp(prefix="labs-gradetest-"))
        grade.CURRENT = {"chapter": "unit", "case": ""}
        self.labs = self.repo / ".labs" / "failures" / "unit"

    def case(self, name, **over):
        c = {"name": name, "binary": "", **over}
        grade.CURRENT["case"] = name
        return c

    def artifacts(self, name):
        d = self.labs / name
        return {p.name: p for p in d.iterdir()} if d.is_dir() else {}

    def test_required_env_absent_fails(self):
        ok, msg = grade.run_case(
            self.repo, self.case("req_env",
                                 binary="{{env:LABS_GRADER_UNSET}}",
                                 required_env=True))
        self.assertFalse(ok)
        self.assertIn("required_env", msg)

    def test_allow_missing_env_skips(self):
        grade.ALLOW_MISSING_ENV = True
        ok, msg = grade.run_case(
            self.repo, self.case("skip_env",
                                 binary="{{env:LABS_GRADER_UNSET}}",
                                 required_env=True))
        self.assertTrue(ok)
        self.assertTrue(msg.startswith("SKIPPED"))

    def test_missing_binary_fails_with_artifact(self):
        ok, msg = grade.run_case(
            self.repo, self.case("bin_missing",
                                 binary="definitely/not/here"))
        self.assertFalse(ok)
        self.assertIn("binary missing", msg)
        self.assertIn("reason.txt", self.artifacts("bin_missing"))

    def test_optional_rom_absent_skips(self):
        ok, msg = grade.run_case(
            self.repo, self.case("rom_skip", binary="/usr/bin/true",
                                 requires_rom="tests/no_rom_exists.bin",
                                 optional=True))
        self.assertTrue(ok)
        self.assertTrue(msg.startswith("SKIPPED"))

    def test_timeout_fails_with_artifacts(self):
        ok, msg = grade.run_case(
            self.repo, self.case("timeout", binary="/bin/sleep",
                                 args=["5"], timeout=1))
        self.assertFalse(ok)
        self.assertIn("timed out", msg)
        arts = self.artifacts("timeout")
        self.assertIn("reason.txt", arts)
        self.assertIn("stdout.txt", arts)
        self.assertIn("stderr.txt", arts)

    def test_crash_signal_fails(self):
        # POSIX: SIGABRT via python. Guarded for non-POSIX platforms
        # (v006 #26: document as unix-only and run conditionally).
        if os.name != "posix":
            self.skipTest("crash/signal test is POSIX-only")
        crash = FIX / "crash.py"
        if not crash.exists():
            crash.write_text(
                "import os, signal\nos.kill(os.getpid(), signal.SIGABRT)\n")
        ok, msg = grade.run_case(
            self.repo, self.case("crash", binary=sys.executable,
                                 args=[str(crash)]))
        self.assertFalse(ok)
        self.assertIn("crashed", msg)

    def test_exit_mismatch_fails(self):
        ok, msg = grade.run_case(
            self.repo, self.case("exit_bad", binary="/bin/sh",
                                 args=["-c", "exit 3"], expect_exit=0))
        self.assertFalse(ok)
        self.assertIn("exit", msg)

    def test_stdout_mismatch_fails(self):
        ok, msg = grade.run_case(
            self.repo, self.case("stdout_bad", binary="/bin/echo",
                                 args=["alpha"], expect_stdout_contains="zzz"))
        self.assertFalse(ok)
        self.assertIn("stdout", msg)

    def test_expected_file_missing_fails(self):
        ok, msg = grade.run_case(
            self.repo, self.case("file_missing", binary="/usr/bin/true",
                                 expect_file_exists=[
                                     str(self.repo/"no_produced.bin")]))
        self.assertFalse(ok)
        self.assertIn("not produced", msg)

    def test_hash_mismatch_fails_and_preserves_artifact(self):
        # v006 #24: the produced file must be PERSISTED under the failure
        # directory so learners can debug the mismatch directly.
        produced = self.repo / "out.bin"
        ok, msg = grade.run_case(
            self.repo, self.case(
                "hash_bad", binary="/bin/sh",
                args=["-c", f"printf abc > {produced}"],
                expect_file_hash={"file": str(produced),
                                  "fnv64": "0000000000000000"}))
        self.assertFalse(ok)
        self.assertIn("hash mismatch", msg)
        arts = self.artifacts("hash_bad")
        self.assertIn("produced.bin", arts)
        self.assertEqual(arts["produced.bin"].read_bytes(), b"abc")

    def test_hash_success_passes(self):
        produced = self.repo / "ok.bin"
        want = grade.fnv1a(b"hello-grader")
        ok, msg = grade.run_case(
            self.repo, self.case(
                "hash_ok", binary="/bin/sh",
                args=["-c", f"printf hello-grader > {produced}"],
                expect_file_hash={"file": str(produced), "fnv64": want}))
        self.assertTrue(ok, msg)


class GraderEndToEnd(unittest.TestCase):
    """grade-last semantics (v006 #27, #11): running the full main() sweep
    serializes PASS / SKIP / FAIL distinctly into .labs/grade-last.json."""

    def setUp(self):
        self.repo = Path(tempfile.mkdtemp(prefix="labs-grade-e2e-"))
        self.hidden = self.repo / "tests" / "hidden" / "galactic"
        self.hidden.mkdir(parents=True, exist_ok=True)
        manifest = {
            "description": "e2e semantics fixture",
            "cases": [
                {"name": "pass_case", "binary": "/bin/sh",
                 "args": ["-c", "exit 0"], "expect_exit": 0},
                {"name": "fail_case", "binary": "/bin/sh",
                 "args": ["-c", "exit 2"], "expect_exit": 0},
                {"name": "skip_case", "binary": "{{env:LABS_GRADER_UNSET}}",
                 "required_env": True},
            ],
        }
        (self.hidden / "manifest.json").write_text(
            json.dumps(manifest))

    def test_grade_last_semantics(self):
        rc = grade.main_cmd(["--repo", str(self.repo),
                             "--allow-missing-env", "galactic"])
        self.assertEqual(rc, 1)          # one failing case -> nonzero
        out = json.loads(
            (self.repo / ".labs" / "grade-last.json").read_text())
        by_name = {r["case"]: r for r in out["results"]}
        self.assertTrue(by_name["pass_case"]["pass"])
        self.assertFalse(by_name["fail_case"]["pass"])
        self.assertFalse(by_name["fail_case"]["skipped"])
        self.assertTrue(by_name["skip_case"]["skipped"])
        # skip is a soft pass, not a failure
        self.assertTrue(by_name["skip_case"]["pass"])


if __name__ == "__main__":
    unittest.main()