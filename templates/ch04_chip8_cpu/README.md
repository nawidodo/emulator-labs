# ch04_chip8_cpu — Complete CHIP-8 CPU

Implements the full CHIP-8 instruction set incrementally: control flow,
register ALU with exact VF rules, quirk-parameterized shifts and memory ops,
deterministic random and keypad handling, plus a course-original flags test
suite executed headless.

## Layout

| Dir | Task (curriculum TODO mapping) |
|---|---|
| `01_control_flow` | TODO1: 1NNN/2NNN/00EE/BNNN + 3XNN/4XNN/5XY0/9XY0 |
| `02_alu` | TODO2+3: 8XY0-8XY7 complete with carry/borrow rules |
| `03_shift_quirks` | TODO4: 8XY6/8XYE under `Chip8Quirks::shift_uses_vy` |
| `04_memory_bcd` | TODO5+6: FX55/FX65 (I quirk), FX33 BCD, FX29 font |
| `05_random_key` | CXNN via seeded LCG + EX9E/EXA1 scripted keys |
| `06_flags_suite` | integrated capstone: runs the committed flags suite ROM |
| `90_debug` | four seeded flag bugs + DEBUGGING.md / bug-report.md |
| `91_challenge` | flags suite green under BOTH quirk profiles |
| `99_coding_test` | unseen MATH-X extension spec (FXY2 et al.) |

Each exercise directory is self-contained (own CMakeLists, own chip8.hpp);
earlier instruction groups arrive pre-implemented so the machine stays
coherent while the @LABS blocks mark each exercise's tasks.

## Runner CLI (all runners)

```
<runner> --rom FILE [--cycles N] [--frames N] --headless
         [--trace FILE] [--regs FILE] [--input-file FILE]
         [--quirks cosmac|modern] [--help]
```

Trace lines: `pc=0200 op=1NNN V0=.. .. VF=.. I=000 SP=00 DT=00 cyc=N`
(post-instruction state; consumed by `tools/labs/compare_trace.py`).
`--input-file`: one line per instruction, hex digits = keys down, `.` = none.
`--frames` aliases cycles here (no framebuffer until ch05).

## Gate checklist

- [ ] exercises: all six `NN_*` dirs go RED (skeleton) -> GREEN (your work)
- [ ] starter: `make skels LABS=ch04_chip8_cpu && make build && make test`
- [ ] debug: fix 90_debug's four seeded bugs; write bug-report.md
- [ ] challenge: 91_challenge green under cosmac AND modern profiles
- [ ] coding_test: hidden manifest passes (`make grade GRADE_TARGETS=ch04_chip8_cpu`)

## Verification

Recorded exactly as run (AppleClang 16, macOS arm64):

```
VERIFY_PREFIX=/tmp/labs-ch04 tools/labs/verify_chapter.sh ch04_chip8_cpu
# -> verdict: skel_build=ok solutions=GREEN   (skeleton: 9/15 RED as designed)
# hidden manifest cases executed directly against scratch binaries:
# 13 passed / 0 failed / 1 skipped (optional requires_rom)
# goldens regenerated twice, byte-identical (cmp); see tests/public provenance
```
