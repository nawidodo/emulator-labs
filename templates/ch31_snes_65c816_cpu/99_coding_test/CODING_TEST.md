# CODING TEST — unseen addressing mode: LDA (dp),Y

Everything you implemented so far used direct, absolute and long
addressing. This test adds one more mode **from a written spec only**:
post-indexed indirect, opcode `$B7`, `LDA (dp),Y`.

Rules of the game:

- Read `SPEC.md`. It is the complete behavioral contract.
- Implement `lda_dp_indirect_y()` in `coding.hpp` (the single
  `TODO(1)` block).
- Do not modify the tests; they encode the spec.
- The hidden grader runs the SAME binary against cases that were not
  shown here — if your implementation follows the spec instead of the
  tests, both pass.

Passing criteria: `ch31_coding_tests` exits 0 in skeleton mode after
your change, and the hidden manifest case `coding_dp_indirect_y` passes
during grading.
