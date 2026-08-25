# Challenge — ch11: CPU Completion Smoke Suite

Two course-original programs (no commercial ROMs; hand-assembled with the
course mini-assembler, see
`tests/public/ch11_gameboy_cpu_completion/fixtures/provenance.md`) run on
the full Chapter 11 machine — CPU with the DAA/CB and stack hooks plus the
IrqHook/IntCtl interrupt model — must reproduce the reference behavior
exactly.

## Acceptance criteria

1. `ch11_91_smoke_tests` passes in the solution configuration: both fixtures
   reach the exact golden final states (registers, SP, PC, cycle count,
   halted flag) and the WRAM side effects line up (service counter at
   `$C100`, stack bytes from PUSH).
2. Full-trace comparison against the committed goldens:

   ```bash
   ./build/skels/ch11_gameboy_cpu_completion/91_challenge/ch11_91_cpu_runner \
       --rom tests/public/ch11_gameboy_cpu_completion/fixtures/smoke_cpu.bin \
       --headless --cycles 100000 \
       --trace /tmp/smoke_cpu.trace.log \
       --hash-frame /tmp/smoke_cpu.final.txt
   python3 tools/labs/compare_trace.py \
       tests/public/ch11_gameboy_cpu_completion/traces/smoke_cpu.trace.log \
       /tmp/smoke_cpu.trace.log
   python3 - <<'PY'
   import pathlib
   def fnv(p):
       h = 0xcbf29ce484222325
       for b in pathlib.Path(p).read_bytes():
           h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
       return f"{h:016X}"
   print("trace", fnv("/tmp/smoke_cpu.trace.log"))
   print("final", fnv("/tmp/smoke_cpu.final.txt"))
   PY
   ```

   Compare the two hashes against `goldens/goldens.md`. Repeat for
   `smoke_irq.bin`.

3. Hidden grading re-runs an unseen fixture variant through this same
   runner and compares the final-state hash.

## What each program exercises

| Fixture | Coverage |
|---|---|
| `smoke_cpu` | DAA all four quadrants (N=0/H, double-adjust +$66, N=1/H, N=1/C), CB-page rotates/shifts/SWAP/BIT/RES/SET flag contracts, PUSH with stack readback round-trip, CALL/CALL nc not-taken/RET z taken, taken and not-taken JR cc timing |
| `smoke_irq` | ISR copy loop down to the $0050 timer vector (BC countdown + JR nz), EI delayed-enable landing after one instruction, priority dispatch (VBlank bit 0 before Timer bit 2), RETI restoring IME, IF/IE as registers via IntBus, WRAM snapshot |

The acceptance runs use this exercise's own solved cores — they stay green
both with the shipped fixtures and after you finish fixing the four seeded
defects in `90_debug/` (the debug excerpts are self-contained copies and do
not feed the machine here).
