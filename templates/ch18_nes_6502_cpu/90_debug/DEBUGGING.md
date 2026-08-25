# 90 — Debugging: Two Bugs Loose in the Addressing Modes

Someone refactored `cpu.hpp` and introduced **two independent bugs**. Your
job is the full debugging loop, not just a fix:

```text
bug                    one sentence
root cause             the exact line and why it is wrong
first divergence       smallest program/trace where behavior differs
fix                    the patch (diff)
regression test        a test that fails before, passes after
```

Write your findings to `bug-report.md` in this directory.

## Symptom A — memory corruption with indexed zero-page stores

A student's sprite engine does `STA $92,X` with X=$F0 and the byte lands at
$0182 instead of $0082. Nothing else seems wrong; non-indexed zero-page
stores behave.

Start here: `mode_zpx` (and its sibling `mode_zpy`) in `cpu.hpp`.

## Symptom B — trace cycles diverge on `(zp),Y`

Comparing against a reference emulator, every `LDA ($40),Y` whose base+Y
crosses a page logs one cycle short (`cyc=5` instead of `cyc=6`). Non-crossing
executions match perfectly, which is why unit tests that never cross missed
it for weeks.

Start here: `mode_izy`'s return value and how `step()` turns it into the
penalty cycle.

## Hints

- The regression suite in `main.cpp` (`TEST(regression, ...)`) encodes the
  correct hardware behavior. It runs RED until you fix both bugs.
- Fix nothing by special-casing call sites — repair the mode functions.
- After fixing, re-run your FULL ch18 suite; both bugs can mask each other.
