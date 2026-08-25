# 13.03 — Overflow: reload and raise

Turn the exercise-02 overflow pulse into interrupt hardware.

## Contract

* `settle_overflow`: consume one pulse — reload TIMA from TMA immediately
  and raise IF bit 2 ($FF0F).
* Real hardware completes the reload 4 T-cycles after the wrap (TIMA reads
  $00 during that window). We implement the IMMEDIATE reload as a
  documented deterministic simplification; see LECTURE.md.
* Writing TMA affects only the NEXT reload; an already-loaded TIMA never
  retro-changes.
* `timer_tick(t, ctl, cycles)` steps in 4-T-cycle blocks, settles any
  overflow, and reports whether the timer fired (drivers log it).

## Target

`ch13_03_reload_tests`
