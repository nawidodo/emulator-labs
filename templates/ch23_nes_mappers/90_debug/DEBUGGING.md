# Debugging — ch23: the MMC3 IRQ that fires early

## Symptom

A game reprograms the MMC3 IRQ period every frame (status-bar splits love
to do this: load a small counter, take the interrupt, restore). With your
mapper, interrupts arrive EARLIER than the reference log whenever a new
period is written while a countdown is already running:

- Periods that are never rewritten behave perfectly (`latch + 1` edges).
- The first interrupt after boot is always on time.
- Any `$C000` write mid-countdown truncates the CURRENT period — the IRQ
  lands as if the counter had been restarted on the spot.

The failing test is `nes23dbg.mid_count_rewrite_keeps_running_the_old_period`.

## Your task

1. Run `ctest` and reproduce the failure.
2. Find the defect in `dbg_mmc3.hpp` (`nes23dbg::Mmc3::cpu_write`, the
   `$C000-$DFFF` even handler).
3. Write `bug-report.md` in this directory containing exactly:
   - **bug**: one sentence,
   - **root cause**: which line misbehaves and what hardware actually does
     (the `$C000` register is a LATCH; consult LECTURE.md, "IRQ model"),
   - **first observable divergence**: the first edge whose counter/IRQ
     state differs from `mid_count_rewrite_keeps_running_the_old_period`,
   - **fix**: the corrected line(s),
   - **regression test**: why the regression test now pins it.

Hint: the fix removes one statement.
