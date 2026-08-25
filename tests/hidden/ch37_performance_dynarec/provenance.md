# Provenance — ch37 hidden fixtures

Authored for this chapter (2026-08), synthetic rx8 images only; `.asm.txt`
sources sit next to each binary (same encoding as the public provenance).

- `programs/bench2.bin` — second optimization-gate workload: XOR-self zero
  fold, SUB-by-r0 identity copy, trip count 15 step −3 (5 iterations).
  Optimizer must clear ≥20% executed-op reduction with an identical dump;
  `ch37_04_runner --check-opt` enforces both internally.
- `programs/ext1.bin` — coding-test workload exercising `mul`/`not`/`min`
  in a countdown loop with per-iteration OUT entries plus one final OUT.

## Golden generation

Identical procedure to `tests/public/ch37_performance_dynarec/provenance.md`
(solution binaries, runner `--dump` files, FNV-64 via hash_frame.py, run
twice byte-identical before committing).
