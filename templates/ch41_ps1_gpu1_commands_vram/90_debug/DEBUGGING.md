# Debugging Exercise — ch41: two seeded GPU raster bugs

The baseline rasterizer "mostly renders" but two hardware contracts broke.
Find both, fix them, file `bug-report.md`.

| # | Site | Hardware truth (PS1 GPU) |
|---|---|---|
| 1 | Drawing-area clip | The area is INCLUSIVE on both corners: a pixel exactly on `x2,y2` must draw. Exclusive-style tests silently delete the last row/column of every primitive. |
| 2 | Rectangle size normalization | Width and height each use their OWN table (`copy_width` / `copy_height`); zero degenerates to that axis' maximum (1024 wide, 512 tall). Mixing the tables floods VRAM with wrong rows. |

## Symptoms

1. Golden frames mismatch by exactly one missing border row+column
   (`debug41.inclusive_clip_keeps_last_row_and_column`).
2. A rectangle with height field 0 paints far more than the full screen
   height; nonzero heights above 512 are also wrong
   (`debug41.zero_height_degenerates_to_full_height`).

## Workflow

```bash
ctest --test-dir build -R ch41_90_debug --output-on-failure
```

Write `bug-report.md` (bug / root cause / first divergence / fix /
regression test). Hidden re-check:
`make grade GRADE_TARGETS=ch41_ps1_gpu1_commands_vram`.
