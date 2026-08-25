# Debugging drill — five seeded raster defects

The skeleton of `raster.hpp` renders *mostly correct* output: the defects
only show under specific timing/sprite/window conditions, exactly how
real PPU bugs hide. All five live in the STUB side; the SOLUTION side is
the corrected code.

| # | Defect | Symptom | Failing suite |
|---|--------|---------|---------------|
| 1 | mode 3→0 threshold computed against the wrong line base (`dot - kDotsPerLine`) | the VRAM/OAM unlock edge never appears inside any visible line in the mode trace — a full line late | `debug_modes.*` |
| 2 | sprite x==0 hide test inverted | visible sprites vanish while hidden (x==0) entries draw shifted on-screen | `debug_sprites.*` |
| 3 | BG-over-sprite priority sense flipped | flagged sprites punch through nonzero background; unflagged sprites hide behind it | `debug_sprites.*` |
| 4 | LYC coincidence compares against LY-1 | STAT IRQ log fires at line LYC-1 instead of LYC | `debug_lyc.*` |
| 5 | window content row derived as LY-WY instead of the internal counter | after a mid-frame disable the window RESTARTS at WY instead of skipping the missed rows | `debug_window.*` |

## Method

1. Run `ch15_90_debug_tests` and pick ONE failing suite.
2. Trace the value flow by hand for that test's inputs.
3. Fix, re-run, then write `bug-report.md`:

```text
bug:
root cause:
first divergence:   (exact input where stub and truth part ways)
fix:
regression test:    (name of the TEST you would add to prevent a relapse)
```

Repeat until all suites pass — do not fix all five blind. The point is
the isolation workflow; the chapter's hidden grade re-checks every fix
through frame/trace hashes and a rerun of these suites.
