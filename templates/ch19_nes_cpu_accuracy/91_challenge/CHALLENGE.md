# 91 — Challenge: Match the Reference Log, Interrupts Included

The ch18 challenge matched a plain instruction trace. This one raises the
bar to the nestest-style log with the accuracy layer switched on: dummy
bus accesses change cycle counts, BRK pushes a different P than IRQ, and
unofficial opcodes show up in the listing by name.

## The fixture

`tests/public/ch19_nes_cpu_accuracy/programs/challenge_prog.bin`
(listing: `challenge_prog.asm.txt`) is a course-original program that runs
on flat RAM at $0600 and installs its own interrupt vectors. It touches:

- `BRK` -> `$FFFE` handler -> counter -> `RTI` round trip (B bit set on
  the stacked P, PC pointing past the padding byte),
- official RMW (`INC $zp`) and unofficial RMW combos (DCP/ISB/SLO/RLA)
  with their double writes,
- LAX/SAX,
- an indexed store (`STA $21FE,X`) with its speculative read at `$21FE`,
  and a page-crossing `LDA $20FF,X` (dummy read at the un-fixed-up
  address),
- `JSR`/`RTS`, a V-overflow `BVS`, taken/not-taken branches, and a final
  JAM opcode that halts the core.

## Your task

1. Finish `trace_line()` in `cpu.hpp` so every line matches the format
   contract in exercise 03.
2. Produce your own log and diff against the committed golden:

   ```bash
   ./ch19_91_challenge_runner \
       --rom tests/public/ch19_nes_cpu_accuracy/programs/challenge_prog.bin \
       --cycles 100 --trace-log /tmp/mine.log
   diff tests/public/ch19_nes_cpu_accuracy/traces/challenge_golden.log /tmp/mine.log
   ```

3. The automated test replays the same program and compares against the
   embedded golden line-by-line; the FIRST divergence is printed — fix
   THAT, not the last symptom.

## Acceptance

- Both tests green; your log diffs clean against the golden (53 lines).
