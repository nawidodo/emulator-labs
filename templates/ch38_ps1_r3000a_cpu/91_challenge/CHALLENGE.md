# CHALLENGE — ch38: run the smoke program

## Task

The runner in this directory is a complete R3000A interpreter (the solution
build). Your job as a student is to have finished exercises 01–03 so the
interpreter works, then verify it end to end:

```bash
./ch38_91_challenge_runner \
    --rom ../../../tests/public/ch38_ps1_r3000a_cpu/roms/cpu_smoke.bin \
    --cycles 2000 \
    --trace /tmp/smoke.trace \
    --hash-frame /tmp/smoke.hash
```

## Acceptance criteria

- The run prints `halted=1` and exits 0.
- The trace matches the committed golden
  `tests/public/ch38_ps1_r3000a_cpu/traces/cpu_smoke.trace` line for line
  (`python3 tools/labs/compare_trace.py <golden> /tmp/smoke.trace`).
- The hash file matches the committed golden hash in the same directory.

## What the fixture exercises

`cpu_smoke.bin` (hand-assembled, see its `.asm.txt` + `provenance.md`) walks
the whole subset: ALU ops, a countdown loop with a taken-branch delay slot,
word/byte stores through KSEG0 addresses, an unaligned lwl/lwr load pair,
mult/div into HI/LO, and a jal/jr call that returns through a delay slot.
It halts with `syscall`.
