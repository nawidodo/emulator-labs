# HUNT — a real compatibility bug, found the way you would in the wild

You have a tiny CHIP-8 program (`divergence.bin`, shipped next to this
chapter's goldens and reproduced in `main.cpp`). It was assembled for a
**COSMAC VIP**-era interpreter. Run under your default **MODERN** profile it
misbehaves: the reference trace (produced by the reference solution running
the same ROM with `--quirks COSMAC_VIP`) disagrees with what your runner
produces.

## Your job

Find the **first divergence**: the earliest trace line where the
wrong-profile run shows state that contradicts the reference trace.
Not the last visible symptom — curriculum §54 is explicit about that.

## The workflow you should practice

1. Produce both traces:

   ```bash
   ./ch06_05_runner --rom tests/public/ch06_chip8_accuracy_debugger/fixtures/divergence.bin \
       --quirks COSMAC_VIP --cycles 8 --trace /tmp/ref.log --trace-full
   ./ch06_05_runner --rom tests/public/ch06_chip8_accuracy_debugger/fixtures/divergence.bin \
       --cycles 8 --trace /tmp/bad.log --trace-full
   python3 tools/labs/compare_trace.py /tmp/ref.log /tmp/bad.log
   ```

   `compare_trace.py` prints the FIRST divergence line and its context.

2. Confirm it interactively with *your* debugger from exercises 02/03:

   ```text
   dbg> break 0204
   dbg> watch V3==12
   dbg> continue
   ```

   Step through, dumping `regs` after every instruction, until a register
   value no longer matches what the reference trace claims for that line.
   Hint: watch I as closely as V — FX55/FX65 disagree about whether I moves,
   and the full-mode reference trace records I on every line.

3. Write down the PC of that line in `compat_answer.hpp`
   (`kFirstDivergencePc`) and make `ch06_05_hunt_tests` pass. The test does
   NOT trust your constant — it re-simulates both profiles and checks your
   answer against the true first divergence.

4. In your notes (or `bug-report.md`, same format as 90_debug), record:
   bug / root cause / first divergence / fix / regression test. Here the
   "fix" is simply selecting the right quirk profile — which is the whole
   point: the emulator was never broken, the *configuration* was.

## Why this matters

Every post-CHIP-8 system (8080, GB, NES) gets its compatibility bugs found
exactly this way: golden trace in one hand, your trace in the other,
bisecting to the first lying line.
