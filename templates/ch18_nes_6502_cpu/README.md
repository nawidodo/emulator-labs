# ch18 — NES 6502 CPU

Build a cycle-accounted 6502 core the way the hardware demands: addressing
modes as independent functions first, opcode semantics second, both glued by
a 256-entry `{mode_fn, op_fn, base, penalty}` decode table.

## Exercises

| Dir | Task |
|---|---|
| `01_addressing_modes` | imm/zp/zpx/zpy/abs/absx/absy/izx/izy/ind + page-cross reporting and zero-page wrap quirks |
| `02_loads_alu` | loads/stores/transfers/stack/logic/ADC/SBC over the dispatch table; official cycle counts |
| `03_flow_stack` | compares, INC/DEC (dummy-write RMW), shifts, branches (2/3/4-cycle rules), JMP/JSR/RTS/RTI/BRK, flag ops |

## Debug

`90_debug` — two seeded bugs: zero-page indexed wrap dropped (`$80,X=$90`
escapes page zero) and `(zp),Y` page-cross penalty lost. Students produce
`bug-report.md` (bug / root cause / first divergence / fix / regression test).

## Challenge

`91_challenge` — run `challenge_prog.bin` (course-original, hand-assembled)
and match the committed reference trace instruction-by-instruction,
including cumulative cycle counts:

```bash
build/skels/ch18_nes_6502_cpu/91_challenge/ch18_91_challenge_runner \
    --rom tests/public/ch18_nes_6502_cpu/programs/challenge_prog.bin \
    --data 2100=0f --cycles 100 --trace /tmp/mine.log
python3 tools/labs/compare_trace.py \
    tests/public/ch18_nes_6502_cpu/traces/challenge_golden.log /tmp/mine.log
```

## Coding test

`99_coding_test` — ten opcode/addressing-mode combinations deliberately
absent from the exercises (`ORA abs,X`, `AND abs,Y`, `EOR (zp,X)`,
`ADC zp,X`, `SBC abs,X`, `CMP (zp),Y`, `INC abs,X`, `DEC abs,X`,
`ROR zp,X`, `BIT zp`) must be added from the spec table alone.

## Gate checklist

- [ ] exercises: skeleton RED -> student GREEN (all three dirs)
- [ ] starter: chapter generates and builds
- [ ] debug: `regression.*` tests green after fixing both bugs + bug-report.md
- [ ] challenge: trace matches golden line-by-line
- [ ] coding test: `unseen.*` green

## Verification

Recorded from authoring run (macOS/arm64, AppleClash C++20):

```
VERIFY_PREFIX=/tmp/labs-NES1 tools/labs/verify_chapter.sh ch18_nes_6502_cpu
[verify] SKEL: build OK; ctest: 14% tests passed, 6 failed out of 7 (red expected)
[verify] SOLUTIONS: GREEN — 100% tests passed out of 7
[verify] verdict: skel_build=ok solutions=GREEN
```

Goldens in `tests/public/ch18_nes_6502_cpu/` were generated twice by the
reference solution (byte-identical); see `traces/provenance.md`.
