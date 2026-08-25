# Challenge — ch10: CPU Smoke Program Suite

Run three course-original programs (no commercial ROMs; hand-assembled, see
`tests/public/ch10_lr35902_cpu/fixtures/provenance.md`) on your CPU and match
the reference behavior exactly.

## Acceptance criteria

1. `ch10_91_challenge_tests` passes in the solution configuration. It runs
   the embedded fixtures and compares final CPU state plus result RAM against
   committed golden values.
2. Full-trace comparison (the real challenge): for each fixture,

   ```bash
   ./build/skels/ch10_lr35902_cpu/03_ld_alu/ch10_03_ld_alu_runner \
       --rom tests/public/ch10_lr35902_cpu/fixtures/sm01_alu_ops.bin \
       --headless --cycles 100000 \
       --trace /tmp/sm01.log
   python3 tools/labs/compare_trace.py \
       tests/public/ch10_lr35902_cpu/traces/sm01_alu_ops.trace.log /tmp/sm01.log
   ```

   Repeat for `sm02_loads_loop` and `sm03_cond_loops`. Every instruction,
   register, and cycle count must line up — a single off-by-one anywhere is
   a bug.

3. Hidden grading (`make grade LABS=...`) re-runs the traces over an unseen
   probe program with a golden hash.

## What the fixtures exercise

| Fixture | Coverage |
|---|---|
| `sm01_alu_ops` | ADD/ADC/SUB/SBC/AND/XOR/OR/CP flags, LDI stores |
| `sm02_loads_loop` | pair loads, LDI/LDD indirects, JR loop, ADD HL,rr |
| `sm03_cond_loops` | nested JR cc / JP cc loops, INC/DEC flag interplay |

Mooneye's acceptance tests are the hardware-grounded follow-up:
https://github.com/retrio/gb-test-roms (requires student-supplied ROM files;
never commit them).
