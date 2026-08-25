# DEBUGGING — 90_debug

The LAB-8 core in this directory is **complete but wrong**: two seeded bugs
hide in `cpu.hpp`. The test suite (`ch02_90_debug_tests`) pins the behavior
required by `SPEC.md` and fails until both bugs are fixed. This is
trace-first debugging practice: the same workflow you will use against real
cores (CHIP-8, 8080, Game Boy) for the rest of the course.

## Setup

```bash
LABS=ch02_emulator_core/90_debug make skels   # or tools/labs/generate.py
make build && ctest --test-dir build -R ch02_90_debug --output-on-failure
```

Reproduce, don't guess: run the failing tests, then write tiny programs and
`--trace` them with the exercise 04 runner to see the first divergence.

## What you get

- `cpu.hpp` — the core. Exactly two handlers are buggy; everything else is
  reference-correct. Both bugs are one or two lines.
- `main.cpp` — the pinning tests. Each bug breaks at least two of them; the
  remaining tests stay green and prove the blast radius is small.
- Symptom reports from a "user" below.

## Symptoms observed

> **Report A (from the loop team):** our countdown program finishes with the
> wrong cycle count and sometimes executes garbage bytes right after a jump.
> Single-stepping shows the pc skipping *forward past* the instruction we
> jumped to. Backward jumps seem worse than forward jumps.

> **Report B (from the comparison team):** code that uses SUB as a
> "less-than" test makes exactly inverted decisions. Equal operands also
> behave oddly: the carry flag lights up even though nothing was borrowed.

## Method (trace-first)

1. Run the failing test with `--output-on-failure`; note which expectation
   diverges FIRST.
2. Write a minimal program (2–4 instructions) that reproduces it.
3. Trace it: dump pc/op/registers per step by calling `step()` in a loop and
   printing `StepResult`.
4. Compare against SPEC.md line by line — the spec is the ground truth, not
   the code.
5. Fix the handler. Do not add compensating hacks elsewhere (no "adjust the
   target at the call site" workarounds); fix the root cause.

## Deliverable: bug-report.md

Create `bug-report.md` next to this file with one section per bug:

```markdown
## Bug 1: <short title>
- Bug: <what is wrong, one sentence>
- Root cause: <why the code does it; name the exact line>
- First divergence: <test name / trace line where reality leaves SPEC.md>
- Fix: <the change you made>
- Regression test: <which TEST() now covers it>
```

Grading: both bugs fixed + all tests green + a complete bug-report.md.
