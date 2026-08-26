#!/usr/bin/env python3
"""Self-tests for the fail-closed verify_current gate (v008 #18/#19).

Covers the observable _verdict() contract:

    any required dimension not green -> ("red", False)   [gate fails]
    all green + integration pending  -> ("incomplete", True)
    all green + integration green    -> ("green", True)
    empty integration                -> ("incomplete", True)

Run with:  python3 -m unittest discover -s tools/labs/tests -v
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "labs"))

import verify_current

GREEN = {
    "reference_ctest": "green",
    "unseen_grade": "green",
    "grader_self_tests": "green",
    "foundations_c17_audit": "green",
}
PENDING = {"ch52": "pending", "ch51": "pending"}


class VerdictContract(unittest.TestCase):
    def test_all_green_pending_integration_is_incomplete_and_passes(self):
        self.assertEqual(verify_current._verdict(GREEN, PENDING),
                         ("incomplete", True))

    def test_all_green_all_integration_green(self):
        self.assertEqual(
            verify_current._verdict(GREEN, {"ch52": "green",
                                            "ch51": "green"}),
            ("green", True))

    def test_any_required_not_green_fails_closed(self):
        for dim in GREEN:
            bad = dict(GREEN, **{dim: "error"})
            overall, ok = verify_current._verdict(bad, PENDING)
            self.assertEqual((overall, ok), ("red", False), dim)
        for status in ("invalid", "not-run"):
            bad = dict(GREEN, reference_ctest=status)
            self.assertEqual(verify_current._verdict(bad, PENDING),
                             ("red", False), status)

    def test_pipeline_dimension_counts_when_present(self):
        with_pipeline = dict(GREEN, pipeline_grade="green")
        self.assertEqual(verify_current._verdict(with_pipeline, PENDING),
                         ("incomplete", True))
        failed_pipeline = dict(GREEN, pipeline_grade="error")
        self.assertEqual(verify_current._verdict(failed_pipeline, PENDING),
                         ("red", False))

    def test_empty_integration_is_incomplete_not_green(self):
        self.assertEqual(verify_current._verdict(GREEN, {}),
                         ("incomplete", True))


class CtestParser(unittest.TestCase):
    """Literal ctest summary tails — proves the happy path without a build."""

    MODERN = ("1/2 Test #1: ok ...   Passed  0.01 sec\n"
              "100% tests passed, 0 tests failed out of 414\n")
    LEGACY = "100% tests passed out of 414\n"
    FAILING = ("98% tests passed, 8 tests failed out of 414\n")

    def test_modern_summary_all_green(self):
        st = verify_current._ctest_state(0, self.MODERN)
        self.assertEqual(st["status"], "green")
        self.assertEqual((st["ran"], st["passed"], st["failed"]), (414, 414, 0))

    def test_legacy_summary_still_green(self):
        st = verify_current._ctest_state(0, self.LEGACY)
        self.assertEqual(st["status"], "green")
        self.assertEqual((st["ran"], st["passed"], st["failed"]), (414, 414, 0))

    def test_failed_tests_are_error_even_with_rc_zero(self):
        st = verify_current._ctest_state(0, self.FAILING)
        self.assertEqual(st["status"], "error")
        self.assertEqual(st["failed"], 8)


    def test_nonzero_returncode_keeps_parsed_failure_counts(self):
        # P2: a failing ctest (rc=8) must still record its real counts
        # instead of discarding the parseable summary for sentinels.
        st = verify_current._ctest_state(8, self.FAILING)
        self.assertEqual(st["status"], "error")
        self.assertEqual((st["ran"], st["passed"], st["failed"]),
                         (414, 406, 8))

    def test_nonzero_returncode_legacy_keeps_counts(self):
        st = verify_current._ctest_state(1, self.LEGACY)
        self.assertEqual(st["status"], "error")
        self.assertEqual((st["ran"], st["passed"], st["failed"]),
                         (414, 414, 0))

    def test_unparseable_output_is_error_not_green(self):
        st = verify_current._ctest_state(0, "some other tool output\n")
        self.assertEqual(st["status"], "error")
class Ch52Status(unittest.TestCase):
    """Extraction testability: _ch52_status runner/golden/rom handling."""

    def _make_layout(self, tmp, *, have_runner=True, have_golden=True, have_rom=True):
        import tempfile
        repo = Path(tmp) / "repo"
        build = Path(tmp) / "build"
        # ensure dirs
        if have_golden or have_rom:
            (repo / "tests/public/ch52_nes_playable_gate/goldens").mkdir(parents=True, exist_ok=True)
            (repo / "tests/public/ch52_nes_playable_gate/roms").mkdir(parents=True, exist_ok=True)
        if have_runner:
            (build / "tools/labs/nes_gate").mkdir(parents=True, exist_ok=True)
            (build / "tools/labs/nes_gate/nes_gate_runner").write_text("#!/bin/sh\nexit 0\n")
            (build / "tools/labs/nes_gate/nes_gate_runner").chmod(0o755)
        else:
            build.mkdir(parents=True, exist_ok=True)
        golden = repo / "tests/public/ch52_nes_playable_gate/goldens/gate_reference.emu_gate"
        rom = repo / "tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes"
        if have_golden:
            golden.write_text(
                "EMU_GATE_V1\nROM_FNV=AAA\nFRAME_FNV=BBB\nAUDIO_FNV=CCC\nPPU_FNV=DDD\nRAM_FNV=EEE\nREPLAY_FNV=\n"
            )
        if have_rom:
            rom.write_text("fake-rom")
        return repo, build

    def test_ch52_status_runner_missing_is_pending(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            repo, build = self._make_layout(tmp, have_runner=False, have_golden=True, have_rom=True)
            self.assertEqual(verify_current._ch52_status(repo, build), "pending")

    def test_ch52_status_golden_missing_is_pending(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            repo, build = self._make_layout(tmp, have_runner=True, have_golden=False, have_rom=True)
            # ensure runner file exists
            self.assertTrue((build / "tools/labs/nes_gate/nes_gate_runner").is_file())
            self.assertEqual(verify_current._ch52_status(repo, build), "pending")

    def test_ch52_status_hash_mismatch_is_error(self):
        import subprocess
        import tempfile
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as tmp:
            repo, build = self._make_layout(tmp, have_runner=True, have_golden=True, have_rom=True)

            def fake_run(*args, **kwargs):
                cur = Path("/tmp/cur_gate_verify.emu_gate")
                cur.write_text(
                    "EMU_GATE_V1\nROM_FNV=AAA\nFRAME_FNV=DIFFERENT\nAUDIO_FNV=CCC\nPPU_FNV=DDD\nRAM_FNV=EEE\nREPLAY_FNV=\n"
                )
                return subprocess.CompletedProcess(args=args[0], returncode=0)

            with patch("verify_current.subprocess.run", side_effect=fake_run):
                self.assertEqual(verify_current._ch52_status(repo, build), "error")

    def test_ch52_status_matching_checkpoint_is_green(self):
        import subprocess
        import tempfile
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as tmp:
            repo, build = self._make_layout(tmp, have_runner=True, have_golden=True, have_rom=True)

            def fake_run(*args, **kwargs):
                cur = Path("/tmp/cur_gate_verify.emu_gate")
                # copy golden exactly -> green
                want = (repo / "tests/public/ch52_nes_playable_gate/goldens/gate_reference.emu_gate").read_text()
                cur.write_text(want)
                return subprocess.CompletedProcess(args=args[0], returncode=0)

            with patch("verify_current.subprocess.run", side_effect=fake_run):
                self.assertEqual(verify_current._ch52_status(repo, build), "green")

    def test_ch52_status_runner_crash_is_error(self):
        import subprocess
        import tempfile
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as tmp:
            repo, build = self._make_layout(tmp, have_runner=True, have_golden=True, have_rom=True)

            def fake_run(*args, **kwargs):
                raise subprocess.CalledProcessError(returncode=1, cmd=args[0])

            with patch("verify_current.subprocess.run", side_effect=fake_run):
                self.assertEqual(verify_current._ch52_status(repo, build), "error")




if __name__ == "__main__":
    unittest.main()
