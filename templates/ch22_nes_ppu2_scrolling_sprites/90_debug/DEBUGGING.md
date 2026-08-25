# Debugging — ch22: the one-scanline scroll that wasn't

## Symptom

Games and test scenes scroll **correctly when the vertical scroll value is a
multiple of 8**, but any fine component is ignored:

- Requesting fine Y = 3 renders as if fine Y = 0 — the picture snaps in
  8-pixel steps only.
- The defect survives the `$2006` round trip: writing t out through
  PPUADDR and reading it back in a debugger shows the coarse part intact
  but the top three bits (fine Y) always zero.
- Horizontal scrolling behaves perfectly, which is why playtests missed it.

The failing test is `nes22dbg.scroll_second_write_packs_fine_y`.

## Your task

1. Run the tests; observe exactly which bits of `t` are wrong.
2. Find the defect in `dbg_scroll.hpp` (`nes22dbg::scroll_write`,
   second-write half).
3. Write `bug-report.md` here with:
   - **bug**: one sentence,
   - **root cause**: which term of the latch equation is missing/wrong and
     where fine Y lives in `t` (LECTURE.md, bit layout),
   - **first observable divergence**: first failing assertion / pixel,
   - **fix**: corrected expression,
   - **regression test**: why the round-trip test now pins it.
