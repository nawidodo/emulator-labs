#!/usr/bin/env python3
"""Self-tests for the generator's path resolution and eligibility logic.

Covers:
    - exercise-level root resolution
    - manifest-ID normalization (directory name vs manifest ID)
    - implementation-status rejection
"""
import tempfile
import unittest
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "labs"))
import generate

class GeneratorResolution(unittest.TestCase):
    def test_manifest_id_normalization(self):
        entries = {"c17_002": {}, "c17_003": {}}
        self.assertEqual(generate.manifest_id_for_dir("c17_002", entries), "c17_002")
        self.assertEqual(generate.manifest_id_for_dir("c17_002_int_model", entries), "c17_002")
        self.assertEqual(generate.manifest_id_for_dir("c17_003_pointers", entries), "c17_003")
        self.assertIsNone(generate.manifest_id_for_dir("c17_999", entries))

    def test_eligibility_enforcement(self):
        tchapters = {
            "c17_002": {"implementation": "verified"},
            "c17_003": {"implementation": "planned"},
        }
        # c17_002 (verified) should pass
        cid = generate.manifest_id_for_dir("c17_002_int_model", tchapters)
        self.assertEqual(tchapters.get(cid, {}).get("implementation"), "verified")
        
        # c17_003 (planned) should be rejected
        cid = generate.manifest_id_for_dir("c17_003_pointers", tchapters)
        self.assertNotEqual(tchapters.get(cid, {}).get("implementation"), "verified")

    def test_planned_external_chapter_rejected_end_to_end(self):
        # End-to-end: --track --targets all --force on an external track
        # whose manifest marks a chapter "planned" must abort with a
        # nonzero exit (via SystemExit) and must NOT generate the dir.
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            track = "ext17"
            tdir = repo / "tracks" / track
            (tdir / "templates" / "c17_004_foo").mkdir(parents=True)
            (tdir / "templates" / "c17_004_foo" / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.21)\n")
            (tdir / "manifest.json").write_text(
                '{"course": "ext17", '
                '"chapters": {"c17_004": {"implementation": "planned"}}}\n')
            out = repo / "out"

            with self.assertRaises(SystemExit) as cm:
                generate.main(["--repo", str(repo), "--track", track,
                               "--force", "--targets", "all",
                               "--out", str(out)])
            self.assertNotEqual(cm.exception.code, 0)

            # The rejected chapter dir must NOT have been generated.
            self.assertFalse((out / "templates" / "c17_004_foo").exists())
            self.assertFalse(list(out.rglob("CMakeLists.txt")))

    def test_external_exercise_resolution_stays_under_track_root(self):
        # Regression pin (review v011 §15/§16): an external
        # chapter/exercise target must resolve inside the track's own
        # templates root, never falling back to the main tree.
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            track = repo / "tracks" / "foundations-c17" / "templates"
            ex = track / "c17_002_integer_model" / "01_probe"
            ex.mkdir(parents=True)

            resolved = generate.resolve_target(
                repo, "c17_002_integer_model/01_probe", track)
            self.assertEqual(resolved, ex)

    def test_external_root_wins_over_conflicting_main_tree(self):
        # Regression pin (review v011 §17): when both the main templates
        # tree and a track contain the same chapter/exercise path, the
        # external call must resolve the TRACK copy (distinct marker file).
        with tempfile.TemporaryDirectory() as td:
            repo = Path(td)
            main_ex = repo / "templates" / "c17_002_integer_model" / "01_probe"
            track_ex = (repo / "tracks" / "foundations-c17" / "templates" /
                        "c17_002_integer_model" / "01_probe")
            main_ex.mkdir(parents=True)
            track_ex.mkdir(parents=True)
            (main_ex / "marker.txt").write_text("main")
            (track_ex / "marker.txt").write_text("track")

            resolved = generate.resolve_target(
                repo, "c17_002_integer_model/01_probe",
                repo / "tracks" / "foundations-c17" / "templates")
            self.assertEqual(
                (resolved / "marker.txt").read_text(), "track")

if __name__ == "__main__":
    unittest.main()
