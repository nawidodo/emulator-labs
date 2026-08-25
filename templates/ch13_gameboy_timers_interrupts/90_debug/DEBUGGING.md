# Debugging drill — three seeded timer defects

The skeleton of `timer_debug.hpp` contains excerpts of the timer unit,
each carrying exactly one seeded defect. Each defect produces
plausible-looking timing — the way real timer bugs hide for weeks in
emulator projects: a clock that ticks, an interrupt that fires, just not
quite when hardware says.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | TAC gate tests bit 0 instead of bit 2 | with `$FF07=$04` (select 00, enabled) TIMA looks frozen; with `$01` (bit 2 CLEAR) it still ticks — timers ignore being switched off or never start at the classic 4096 Hz rate | `debug_gate.*` |
| 2 | TAC write path has no falling-edge evaluation on DISABLE | switching the timer off while the tapped DIV bit reads 0 loses exactly one tick; fast poll loops that toggle TAC under-count | `debug_disable.*` |
| 3 | Overflow path reloads TMA and then ALSO wraps to $00 ("double reload") | after every overflow TIMA reads $00 instead of TMA, so periodic ISRs drift apart by a full TIMA period | `debug_reload.*` |

## Method

1. Run `ch13_90_debug_tests` and pick ONE failing suite.
2. Trace the value flow by hand for that test's inputs.
3. Fix, re-run, then write `bug-report.md`:

```text
bug:
root cause:
first divergence:   (exact input where stub and truth part ways)
fix:
regression test:    (name of the TEST you would add to prevent a relapse)
```

Repeat until all tests pass. Do not fix all three blind — the point is
the isolation workflow.
