# Hint Escalation & Solution Policy

Every exercise ships its full reference implementation inside the template
(`@LABS-SOLUTION` blocks). It stays unavailable in your skeleton until you
earn it — or ask for it.

## Ladder

```
Hint 0  concept involved          (EXERCISES.md per exercise)
Hint 1  relevant structure/function
Hint 2  relevant algorithm
Hint 3  pseudocode
Hint 4  partial implementation
Give up -> complete reference solution:
        python3 tools/labs/generate.py --targets chNN/NN_ex --todo <last>
```

Hints live at the bottom of each exercise's `EXERCISES.md` (or `SPEC.md`),
collapsed under spoiler markers so you can read incrementally.

## SOLVED WITH REFERENCE

If you reveal a full solution, that task is marked
`SOLVED WITH REFERENCE` in progress notes. The gate still requires a PASS on
an unseen coding test covering the same concept — the test is the arbiter,
not the struggle.

## Debugging exercises are different

For `90_debug` you must produce `bug-report.md` with exactly:

```text
bug:                  <one line>
root cause:           <mechanism, not symptom>
first divergence:     <pc/cycle where good and bad traces split>
fix:                  <diff summary>
regression test:      <test name you added or existing test now covering it>
```

Trace-first workflow (curriculum §54): run your build and the reference trace
through `tools/labs/compare_trace.py`, find the FIRST divergence, never chase
the last visible symptom.
