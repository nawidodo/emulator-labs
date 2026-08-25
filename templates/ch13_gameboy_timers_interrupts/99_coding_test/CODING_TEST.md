# Coding test — the three edge cases

The hidden grader (`make grade GRADE_TARGETS=ch13_gameboy_timers_interrupts`)
re-runs the debug suites and pushes **unseen** timer programs through your
`ch13_91_timer_runner`, hashing the interrupt log. There is nothing new to
implement here — this file describes the defects behind the hidden fixtures
and the method that isolates them fast.

## Setup

Finish exercises 01-04, fix all three `90_debug` defects (bug-report.md
written), then:

```bash
make grade GRADE_TARGETS=ch13_gameboy_timers_interrupts
```

## The three edge cases (as behavioral contracts)

1. **Gate bit.** TAC bit 2 is the ONLY enable. A timer that runs with
   `$FF07=$05` but not `$04` (or worse: runs when bit 2 is clear because it
   tested a select bit) fails `coding.gate_bit_is_tac_bit2_only`.
2. **Disable edge.** Disabling TAC while the tapped DIV bit reads 0 after
   the write produces exactly ONE TIMA increment. Zero increments loses
   ticks in every poll loop that toggles the gate; two increments double
   them. Pinned by `coding.disable_with_tapped_bit_low_produces_one_tick`.
3. **Single reload.** On overflow TIMA reloads TMA exactly once and raises
   IF bit 2 exactly once. A "double reload" leaves TIMA at $00, so periodic
   interrupts drift apart by a full TIMA period.
   Pinned by `coding.overflow_reloads_once_and_raises_if_once`.

## Methodology: log-first divergence isolation

1. **Reproduce with the interrupt log, never with vibes.** Run the failing
   fixture through the runner with `--trace /tmp/irq.log`. Timing bugs show
   up as a diffable text divergence long before anything else looks wrong.
2. **Find the FIRST diverging line.** Diff against the committed public
   golden under `tests/public/ch13_gameboy_timers_interrupts/goldens/`.
   A missing `tima_overflow` line points at the gate or the edge detector;
   an extra one points at the disable edge or a phantom mux fall; wrong
   spacing between lines points at the reload policy.
3. **Map divergence to hardware cause.** Re-derive the invariant from Pan
   Docs' timer chapter rather than special-casing the fixture — every
   defect here is a one-line wrong invariant.
4. **Fix the model, re-grade.** All non-optional hidden cases must pass;
   the optional mooneye case skips honestly unless you supply the ROM.
