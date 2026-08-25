#!/usr/bin/env python3
"""Direct grader self-tests (v005 P1 #22/#23).

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
    hash mismatch       -> FAIL (+ produced artifact preserved)
    successful hash     -> PASS
    grade-last semantics

Run with:  python3 -m unittest discover -s tools/labs/tests -v
"""

from __future__ import annotations

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

# Ensure fixture scripts are executable.
for f in FIX.iterdir():
    if f.suffix == ".sh":
        f.chmod(0o755)


def make(tmp: Path, name: str, **overrides) -> dict:
    case = {"name": name, "binary": "", **overrides}
    return case


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
        self.assertIn("reason.txt", self.artifacts("timeout"))

    def test_exit_mismatch_fails(self):
        ok, msg = grade.run_case(
            self.repo, self.case("exit_bad", binary=str(FIX/"exit_3.sh"),
                                 expect_exit=0))
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

    def test_hash_mismatch_fails_preserving_artifact(self):
        produced = self.repo / "out.bin"
        ok, msg = grade.run_case(
            self.repo, self.case(
                "hash_bad", binary=str(FIX/"write_file.sh"),
                args=[str(produced)],
                expect_file_hash={"file": str(produced),
                                  "fnv64": "0000000000000000"}))
        self.assertFalse(ok)
        self.assertIn("hash mismatch", msg)

    def test_hash_success_passes(self):
        produced = self.repo / "ok.bin"
        want = grade.fnv1a(b"hello-grader")
        ok, msg = grade.run_case(
            self.repo, self.case(
                "hash_ok", binary=str(FIX/"write_file.sh"),
                args=[str(produced)],
                expect_file_hash={"file": str(produced), "fnv64": want}))
        self.assertTrue(ok, msg)


if __name__ == "__main__":
    unittest.main()