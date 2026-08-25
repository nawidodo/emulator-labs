# Debugging drill — three seeded BG-render defects

The skeleton of `renderer.hpp` renders *mostly correct* frames: the
defects only show under specific register/scroll conditions, which is
exactly how real PPU bugs hide.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | BGP fields read in reverse order | frame looks like a negative whenever BGP != $E4 | `debug_bgp.*` |
| 2 | tilemap coordinates not wrapped mod 32 | garbage tiles when scroll pushes indices past 31; reads outside the 1 KB map | `debug_map.*` |
| 3 | $8800 signed tile addressing ignored | games using signed tile bytes draw from the wrong end of VRAM (map byte $FF = tile -1, not +255) | `debug_tiles.*` |

## Method

1. Run `ch14_90_debug_tests` and pick ONE failing suite.
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
the isolation workflow, which chapter 15's raster drills scale up.
