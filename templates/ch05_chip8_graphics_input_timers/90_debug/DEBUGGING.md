# DEBUGGING — ch05 seeded defects

The skeleton of `90_debug` contains **three** deliberately seeded bugs in the
CHIP-8 graphics/timer core (`machine.hpp`). The unit suite in `main.cpp`
fails against them. Your job is not just "fix it" — it is to produce
`bug-report.md` with, for each bug:

```text
bug               one-line description of the misbehaviour
root cause        the exact line(s) and why they are wrong
first divergence  the smallest input where behaviour differs from hardware
fix               what you changed
regression test   which test in main.cpp (or a new one) now guards it
```

## Symptoms (start here — do not read the source first)

### Bug A — "collision flag lies"

- Games that rely on VF after DXYN behave inverted: sprites report a hit on
  every clean draw onto empty screen, and *fail* to register when a sprite is
  wiped over an existing one.
- Unit tests: `bug1_collision.*`.

### Bug B — "edge sprites lose whole rows"

- Draw any sprite whose right edge crosses x=64 in clip mode: rows that
  should render partially vanish completely instead of being clipped at the
  screen border.
- First divergence: a single 8x1 row `0xF0` at origin (62, y) renders
  nothing; correct CHIP-8 lights columns 62-63.
- Unit tests: `bug2_clip.*`.

### Bug C — "everything runs at double speed"

- Delay-based pacing burns twice as fast: a program that sets DT=60 for a
  one-second wait finishes in half a second. Beeps are half as long too.
- Root cause hides inside a single timer function — count how many times the
  counters move per tick.
- Unit tests: `bug3_timers.*`.

## Workflow

1. Build the failing suite, confirm all three groups fail:
   ```bash
   ctest --test-dir build -R ch05_90 --output-on-failure
   ```
2. For each bug group, write the bug-report section BEFORE fixing.
3. Fix `machine.hpp`, re-run until green.
4. Add one extra regression test per bug that would have caught your
   favourite alternative wrong fix.

## Reference

The reference solution keeps these behaviours:

| Behaviour | Contract |
|---|---|
| VF after DXYN | 1 iff any lit pixel was erased by the XOR |
| Clipping | per-pixel: off-screen pixels dropped, rest drawn |
| Wrapping quirk | coordinates taken modulo screen size |
| Timer rate | exactly one decrement per 10 CPU cycles (600 cps / 60 Hz) |
