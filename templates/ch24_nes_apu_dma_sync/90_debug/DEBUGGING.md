# Debugging — ch24: the DMA that completes a cycle early

## Symptom

Everything downstream of a sprite DMA lands ONE CPU cycle (three PPU
dots) early. Raster splits written right after `$4014` shift left by one
cycle — invisible in screenshots of static scenes, glaring in scroll
sweeps. Audio stays byte-identical; only the timing bill is wrong.

The failing tests are `nes24dbg.dma_bill_is_513_from_even_start`,
`nes24dbg.dma_bill_is_514_from_odd_start`, and the consequence test
`nes24dbg.raster_shift_after_dma_matches_hardware_accounting`.

## Your task

1. Run `ctest` and reproduce the failures.
2. Find the defect in `dbg_dma.hpp` (`nes24dbg::run_oam_dma`).
3. Write `bug-report.md` in this directory containing exactly:
   - **bug**: one sentence,
   - **root cause**: which token miscounts and why hardware spends it
     (the final OAM write is a full cycle; consult LECTURE.md,
     "OAM DMA"),
   - **first observable divergence**: which assertion fails first and by
     how much,
   - **fix**: the corrected expression,
   - **regression test**: why the two exact-bill tests now pin it.

Hint: the fix restores one term.
