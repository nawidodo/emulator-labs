# 99_coding_test — Unseen Transfer Pattern: "Mode X"

This is the chapter's specification test: a DMA transfer pattern that does
NOT exist on real hardware is specified below and in SPEC.md. You implement
it from the spec alone — no lecture section covers it, no earlier exercise
prepares it. Read SPEC.md, then complete the two `TODO` blocks in
`99_coding_test/coding.hpp`.

## Contract

- Public tests in `99_coding_test/main.cpp` (suite `VariantSpec`) encode
  every example from SPEC.md.
- The hidden grader runs your built `ch33_99_coding_tests` binary with
  filter argument `VariantSpec`; exit code 0 = pass.
- The stub compiles but returns wrong values, so the skeleton tree fails
  these tests by design.

## How to approach it

1. Read SPEC.md top to bottom. The worked example IS the test vector.
2. Note the two twists vs ordinary DMA: the `+2,+1` offset tail, and the
   A step that ignores control bits 4-3 entirely.
3. Implement, run `ctest -R ch33_99_coding_tests`, compare against the
   SPEC.md table row by row if anything differs.

Time-box: this exercise is designed to take under an hour when the spec
is read carefully first.
