# ch25 — ARM7TDMI ARM Instruction Set

Build the ARM-side brain of the GBA: condition codes, the barrel shifter,
the data-processing group, single load/store, and branches. The reference
CPU keeps **shifter carry** separate from **ALU carry** (curriculum point).

## Exercises

| Dir  | Topic            | Deliverable |
|------|------------------|-------------|
|01_conditions | all 16 condition fields | `cond_pass()` truth table |
|02_shifter   | LSL/LSR/ASR/ROR/RRX, imm + register amounts, exact carry-out | `barrel_shift` family |
|03_data_processing | DP group with S-flag semantics (shifter carry vs ALU carry) | mini ALU |
|04_loadstore | LDR/STR/LDRB/STRB, pre/post/writeback, unaligned rotate | load/store unit |
|05_branches  | B/BL/BX, LR = pc-4 semantics, cycle counts + headless runner | branch unit + runner |
|90_debug     | five seeded carry/ALU/LR bugs | DEBUGGING.md + bug-report.md |
|91_challenge | LDM/STM block transfer + synthetic ARM test program vs golden dump | CHALLENGE.md |
|99_coding_test | unseen instruction family: SWP/SWPB + MRS/MSR from spec | CODING_TEST.md |

## Runner

`05_branches`, `91_challenge` and `99_coding_test` build a headless runner:

```bash
ch25_runner --rom prog.bin --headless --cycles 200 --trace t.log --dump d.txt
```

Trace lines follow the canonical format (`pc=<hex> op=<hex> ... cyc=<n>`);
`--dump` writes the final register/CPSR state for golden hashing.
For this CPU-only chapter `--frames N` behaves like `--cycles N`.

## Verification

Recorded at the end of authoring; see the per-chapter gate in README bottom.

## Verification

```
VERIFY_PREFIX=/tmp/labs-gba1 tools/labs/verify_chapter.sh ch25_arm7tdmi_arm_isa
# -> skel_build=ok solutions=GREEN
python3 tools/labs/grade.py --repo . ch25_arm7tdmi_arm_isa
# -> all non-optional hidden cases pass
```
