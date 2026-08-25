# DEBUGGING — ch38: five delay-slot defects

The interpreter in `interp_debug.hpp` is byte-identical to the ch38_03
reference except for **five seeded bugs**, all inside the small helper
functions at the top (everything below them is known-good). The test suite
fails RED until all five are fixed.

Your job is the full debugging loop from curriculum §59. For each bug,
record in `bug-report.md`:

```text
bug                   one line
root cause            which function, what the code does vs. should
first divergence      the FIRST observable wrong step, not the symptom
fix                   what you changed
regression test       which TEST now covers it
```

## Symptom catalogue

Run `ch38_90_debug_tests` and match failures to symptoms:

| # | Area | Symptom you will observe |
|---|------|--------------------------|
| 1 | window advance | Taken branches fall through past their delay slot; untaken branches jump to a stale target. Programs "fall off" after any branch. |
| 2 | branch base | Every loop lands one instruction off; backward branches re-run or skip the branch itself. |
| 3 | call link | A function's own delay-slot instruction executes a second time when the function returns. |
| 4 | register jumps | The first instruction at a `jr` destination is skipped entirely. |
| 5 | tracer flag | Traces and StepResult claim no instruction ever runs in a delay slot, even though flow is otherwise correct. |

## Hints

- The whole delay-slot mechanism lives in five tiny pure functions; write
  unit calls against each before running programs.
- Remember the MIPS rule: branch displacements are relative to the DELAY
  SLOT address (`pc + 4`). Jumps and branches share the same window update.
- `jal` links past the slot so that returning never re-executes it.

## Done when

`ch38_90_debug_tests` passes fully AND your `bug-report.md` documents all
five defects with root cause + first divergence + fix + regression test.
