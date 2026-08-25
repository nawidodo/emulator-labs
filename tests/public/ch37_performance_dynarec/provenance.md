# Provenance — ch37 fixtures

All programs are hand-assembled synthetic rx8 images authored for this
chapter (2026-08; no commercial content, no external suites). The
`.asm.txt` files next to each `.bin` are the authoritative sources; the
binaries were produced by the chapter's two-pass assembler (encoding:
`op[31:24] rd[23:20] rs[19:16] rt[15:12] imm12[11:0]`, little-endian,
branch/jump targets absolute as `imm12 << 2`).

- `programs/bench.bin` — benchmark workload: 3 setup + 6 iterations of an
  8-instruction loop + OUT + HALT = **53 executed instructions** on the
  switch interpreter. Deliberately full of optimizer bait (identity copies,
  dead constant) and one fusion candidate (`addi r7,r7,-2 ; bnez r7`).
- `programs/smc.bin` — self-modifying-code fixture: the OUT at 0x08 runs on
  pass 1, then a store patches that word into `ADDI r2,r0,9` and jumps back.
  Correct pipelines print exactly one OUT entry (111); a cache that skips
  invalidation prints three.

## Golden generation

```bash
python3 tools/labs/generate.py --mode solution --force --targets ch37_performance_dynarec
cmake -S . -B build-solutions -DLABS_BUILD_SOLUTIONS=On && cmake --build build-solutions -j
B=build-solutions/solutions/ch37_performance_dynarec
$B/01_dispatch_bench/ch37_01_runner --program tests/public/ch37_performance_dynarec/programs/bench.bin \
    --trace /tmp/bench.trace --dump /tmp/bench.dump
python3 tools/labs/hash_frame.py /tmp/bench.trace --fnv-only
python3 tools/labs/hash_frame.py /tmp/bench.dump --fnv-only
```

Run twice; byte-identical before committing (verified: DETERMINISTIC — all
scores are executed-instruction counts, no wall time anywhere).
