#!/usr/bin/env bash
# ch52 out-of-the-box self-check: verifies the grading pipeline itself
# (manifest parsing, env indirection handling, hash tooling) without any
# student implementation. Always passes on a healthy checkout.
set -e
REPO="$(cd "$(dirname "$0")/../../../.." && pwd)"
[ -f "$REPO/docs/final-challenge.md" ] || exit 1
[ -f "$REPO/templates/ch52_nes_playable_gate/99_unseen_system_test/CODING_TEST.md" ] || exit 1
python3 - <<'PY'
import json, hashlib
from pathlib import Path
p = Path("templates/ch52_nes_playable_gate/99_unseen_system_test/manifest.example.json")
spec = json.loads(p.read_text())
assert all(c["binary"].startswith("{{env:LABS_NES_GATE_BIN}}") for c in spec["cases"][:2])
probe = b"ch52-pipeline-ok"
print("FNV64", hashlib.sha256(probe).hexdigest()[:16].upper())
PY
