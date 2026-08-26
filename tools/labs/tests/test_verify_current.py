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

    def test_nonzero_returncode_is_error(self):
        st = verify_current._ctest_state(1, self.MODERN)
        self.assertEqual(st["status"], "error")

    def test_unparseable_output_is_error_not_green(self):
        st = verify_current._ctest_state(0, "some other tool output\n")
        self.assertEqual(st["status"], "error")



if __name__ == "__main__":
    unittest.main()
