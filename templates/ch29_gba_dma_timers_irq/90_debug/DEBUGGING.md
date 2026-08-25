# DEBUGGING — ch29: the one-line-late DMA and the short timer

Two independent timing defects are seeded in `race.hpp`. Games would show:

1. **HBlank effects applied to the wrong scanline.** Per-scanline palette or
   sprite tweaks (raster effects, water reflections) are shifted by a whole
   line; mid-frame wobble appears. The event trace shows HBlank DMA at
   cycles that drift exactly one line away from `line*1232 + 960`.
2. **Audio/sample timers run fast.** A Direct Sound timer with reload R
   overflows sooner than `0x10000 - R` ticks — pitch is slightly high and
   every downstream FIFO refill happens early.

## Method (trace-first)

Run the 91_challenge fixture with `--trace` against a correct build and the
bugged build; diff the traces. The FIRST divergence pinpoints which event
fires at which wrong cycle — resist staring at final symptoms.

## Your task

1. Identify both defects in `race.hpp` (one relational/constant error each).
2. Fix them.
3. Write `bug-report.md`: bug / root cause / first divergence (exact cycle) /
   fix / regression test.

The regression tests already exist in `main.cpp`; fix the root cause.
