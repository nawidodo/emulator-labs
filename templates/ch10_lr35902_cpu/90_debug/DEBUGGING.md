# Debugging Exercise — ch10 / 90_debug

`cpu_dbg.hpp` implements the Chapter 10 instruction subset but carries
**three seeded bugs**. The unit tests in `main.cpp` fail until you find and
fix all of them.

## Symptoms (observed on the buggy skeleton)

Run `ch10_90_debug_tests` and study each failing assertion:

1. **Chained adds come out one low.** A test sets the carry flag, runs
   `ADC A,1`, and expects the carry to be included in the sum.
2. **A compare destroys the accumulator.** After `CP n`, register A holds
   the comparison result instead of its original value — any code that uses
   CP inside a loop corrupts its counter.
3. **Conditional jumps cost too much when they fall through.** A `JR cc,e`
   that is *not* taken is charged the taken price (`cycles_alt`). Compare
   total cycle counts of a fall-through program against the expected 20.

## Method (curriculum §54)

Trace first: diff a failing program's execution against your expectations
instruction by instruction. Find the FIRST divergence, not the last symptom.

## Deliverable

Fix `cpu_dbg.hpp` (the STUB side of blocks 1–3 is the buggy variant; the
SOLUTION side shows correct behavior — no peeking until you've written a
hypothesis!) and write `bug-report.md` in this directory with:

```
bug:            <one line per defect>
root cause:     <code-level explanation>
first divergence: <test name + line>
fix:            <what you changed>
regression test:<which TEST() now covers it>
```
