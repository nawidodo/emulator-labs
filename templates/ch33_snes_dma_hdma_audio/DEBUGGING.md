# 90_debug — Debugging Guide: the One-Line-Late Gradient

## Symptom

The HDMA brightness gradient on screen (and in the per-line effect buffer)
is shifted DOWN by one scanline:

- Scanline 0 renders with no effect applied at all (brightness stays at
  whatever it was before, here 0).
- Scanline N>0 renders with scanline N-1's intended value.
- The very last line's final value never appears where expected; the
  gradient looks "dragged" toward the bottom of the frame.

In trace terms: the first register write appears with `line=1` instead of
`line=0`, and every logged value is the previous line's.

## Reproduction

```bash
# skeleton tree (bug present):
cmake --build build --target ch33_90_debug_tests
ctest --test-dir build -R ch33_90_debug_tests --output-on-failure
```

`DebugGradient.FullLogMatchesGolden` fails immediately.

## Hint ladder (try each before reading the next)

1. **Hint 1** — The golden log in `golden/gradient_writes.log` says line 0
   must carry a write. Does your log have one?
2. **Hint 2** — Compare `log[1].value` against the golden. Which line's
   table entry does it hold? What index expression would produce that?
3. **Hint 3** — The bug is not in the table and not in the ramp math.
   Look at WHEN the engine applies an entry relative to the scanline that
   consumes it: end-of-line application means line n+1 sees it.
4. **Hint 4** — Re-read the timing contract in `02_hdma/hdma.hpp`
   (`run_line`): effects apply at line START. One index in
   `90_debug/hdma_line.hpp` contradicts it.

## First observable divergence

**Line 0**: the golden requires `{line=0, val=00}` as the FIRST log row;
the buggy engine emits NO write for line 0 at all. That missing row is the
cleanest tell.

**Line 14**: the first VALUE-level divergence between rows that exist in
both logs. The ramp is `min(15, n*16/224)`, holding each brightness level
for 14 lines: golden says `val=01` (ramp(14)), while the buggy log still
carries `val=00` — the stale `ramp(13)`. From line 14 onward every buggy
row equals `ramp(n-1)` instead of `ramp(n)` until both saturate near the
frame tail; earlier rows coincide only because ramp(0..13) is a plateau.

## Root cause

`GradientHdma::run()` applies each table entry at the END of its line:
it skips line 0 entirely (`if (n == 0) continue;`) and records
`table[n-1]` for every later line. Hardware applies HDMA writes at the
START of the scanline they belong to.

## Fix

```cpp
for (int n = 0; n < kLines; ++n) {
    log.push_back({n, table_[size_t(n)]});   // apply at line START
}
```

No offset, no skip. Line n's entry lands in the log with line n.

## Regression test

Already present: `DebugGradient.LineZeroGetsItsValue` and
`DebugGradient.LineOneCarriesItsOwnData` fail under the bug and pass after
the fix. Keep them; add your own if yours caught something these did not.

## Deliverable

Write `bug-report.md` in this directory:

```text
bug:            one-scanline-late HDMA effect application
root cause:     <your words>
first divergence: <exact line + expected vs actual>
fix:            <the change you made>
regression test: <test(s) that fail before / pass after>
```
