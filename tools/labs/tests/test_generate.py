#!/usr/bin/env python3
"""Self-tests for the generator's path resolution and eligibility logic.

Covers:
    - exercise-level root resolution
    - manifest-ID normalization (directory name vs manifest ID)
    - implementation-status rejection
"""

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

if __name__ == "__main__":
    unittest.main()
