# Debugging Exercise — The Case of the Drifting Cycles and the Wandering PC

Your chapter 8 CPU passes the simple call/return tests but a real program
misbehaves: cumulative cycle counts run low, and some returns jump into
padding instead of back to the caller.

Two bugs are seeded in `90_debug/cpu.cpp`. Both are marked with
`BUG(1)` / `BUG(2)` comments in the generated skeleton (the comments are
removed in your fixed copy — you find them yourself in practice).

## Symptoms

1. **Trace divergence at the first conditional CALL.** Compare your trace
   against `tests/public/ch08_i8080_control_interrupts/traces/` goldens
   with `tools/labs/compare_trace.py` — the first differing line is the
   conditional call itself (`cyc` field), everything after inherits the
   drift. Taken/not-taken cycle counts are swapped.
2. **Returns land on byte-swapped addresses.** A return pushed from 0x0005
   resumes at 0x0500. Execution wanders into zero-padding (NOPs) or stops.
   Any path through the shared pop helper is affected — including
   POP PSW, where the accumulator comes back as the flags byte.

## Your task

1. Reproduce both failures with `ctest -R ch08_90_debug` (RED on skeleton).
2. For each bug record:
   ```text
   bug:
   root cause:
   first observable divergence:   (trace line or instruction address)
   fix:
   regression test:
   ```
3. Fix the code until the suite is GREEN.
4. Commit `bug-report.md` next to this file's directory in your solution.

## Hint

The trace differ is your friend — curriculum §54: hunt the FIRST
divergence, not the last visible symptom. The cycle bug is visible one
instruction before anything else looks wrong.
