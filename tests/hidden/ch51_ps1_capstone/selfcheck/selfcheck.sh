#!/usr/bin/env bash
# ch51 grading-pipeline self-check (runs out-of-the-box, no student binary).
#
# Validates the three pieces of grade.py machinery the capstone flow
# depends on: committed-executable resolution from a manifest "binary"
# field, {{tmp}} scratch-dir expansion in args, and FNV-1a-64 file
# hashing of a produced artifact.
#
# Usage: selfcheck.sh <outfile>
# Writes a fixed, deterministic payload to <outfile>. The expected
# digest is pinned in tests/hidden/ch51_ps1_capstone/manifest.json.
set -eu

out="${1:?usage: selfcheck.sh <outfile>}"

cat > "$out" <<'PAYLOAD'
ch51 capstone grading pipeline self-check
pipeline=ok
binary=resolved-from-manifest
tmpdir=expanded
hasher=fnv1a-64
PAYLOAD
