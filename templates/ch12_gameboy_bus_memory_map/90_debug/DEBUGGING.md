# Debugging drill — three seeded bus defects

The skeleton of `bus_debug.hpp` contains self-contained excerpts of the
chapter's routing machinery, each carrying exactly one seeded defect.
Each produces plausible-looking bus traffic — the way real memory-map
bugs hide for weeks in emulator projects.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | Echo writes land in a detached shadow buffer while reads alias correctly | state stashed through E000-FDFF silently vanishes; read-only echo users work fine, so the corruption surfaces only after a reboot | `debug_echo.*` |
| 2 | FF50 handler compares wrong operands (checks the *value* against `$50` / routes `$FF4F`) | boot overlay persists no matter what the game writes — or drops early on an unrelated register write | `debug_boot.*` |
| 3 | Unusable page FEA0-FEFF backed by scratch RAM | reads return the last-written byte instead of the documented `$00`; scratch canaries see phantom values | `debug_gap.*` |

## Method

1. Run `ch12_90_debug_tests` and pick ONE failing suite.
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
the isolation workflow. Note that the excerpts deliberately do NOT
include the exercise headers: real bugs live in code that looks
familiar but is not quite your code.
