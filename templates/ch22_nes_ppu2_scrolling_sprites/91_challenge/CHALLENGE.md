# Challenge — ch22: raster-timing snapshot tests (course-original)

Real raster effects (status bars, split-screen scrolling) change scroll
mid-frame and depend on EXACT dot timing of the loopy registers. This
challenge ships a course-original timing model (`timing.hpp`): your job is
to make it pass against reference snapshots taken at precise (line, dot)
points.

## Model rules (documented, rendering enabled)

- dots 8..256 step 8 plus 328/336: `increment_x`
- dot 256: `increment_y`; dot 257: `copy_x`
- pre-render line 261, dots 280..304: `copy_y`

## Task

Run the timing runner over each script and reproduce the reference
snapshot hashes:

| Script | FNV64 of snapshot stream |
|---|---|
| `fixtures/split_status_bar.txt` | `2BDC4EA0E4CB3D2C` |
| `fixtures/fine_scroll_sweep.txt` | `177E961AC7918A29` |

```bash
./ch22_91_timing_runner --script fixtures/split_status_bar.txt --trace /tmp/s.txt
python3 tools/labs/hash_frame.py /tmp/s.txt --fnv-only
```

## Acceptance criteria

- Both snapshot-stream hashes match.
- You can explain WHY a mid-line $2005/$2006 write sequence produces the
  exact v/x/w values in the trace — point at the dot table in LECTURE.md.
