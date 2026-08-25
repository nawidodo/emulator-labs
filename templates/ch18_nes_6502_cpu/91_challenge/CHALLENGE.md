# 91 — Challenge: Match the Reference Trace

The acceptance bar for a CPU core is simple: run a program and produce the
exact same instruction-by-instruction record as a known-good reference —
registers, flags, stack pointer and **cumulative cycle count** on every line.

## The fixture

`tests/public/ch18_nes_6502_cpu/programs/challenge_prog.bin`
(listing: `challenge_prog.asm.txt`) is a course-original program that touches
every tricky corner from this chapter:

- zero-page INC with its 5-cycle dummy write,
- an LSR loop with taken (3 cyc) and not-taken (2 cyc) branches,
- a page-crossing `LDA $20FE,X` (+1 penalty),
- `JSR`/`RTS` with the pc-1 return convention,
- signed overflow (`ADC #$80 + #$80`) driving a `BVS`,
- `JMP ($0650)` through an indirect vector.

It runs on flat RAM: load at $0600, preload `$2100 = $0F`.

## Your task

1. Finish the three marked gaps in `cpu.hpp` (`mode_ind` page wrap, branch
   cycle fixups, step() penalty billing).
2. Run the reference runner and diff yourself:

   ```bash
   ./ch18_91_challenge_runner --rom tests/public/ch18_nes_6502_cpu/programs/challenge_prog.bin \
       --data 2100=0f --cycles 100 --trace /tmp/mine.log
   python3 tools/labs/compare_trace.py \
       tests/public/ch18_nes_6502_cpu/traces/challenge_golden.log /tmp/mine.log
   ```

3. The automated test in this directory replays the same program and compares
   against the committed golden trace line-by-line. First divergence is
   printed — fix THAT, not the last symptom.

## Acceptance

- Both tests green; `compare_trace.py` reports zero divergences.
