# Debugging drill — four seeded CPU defects

`debug_cpu.hpp` contains self-contained excerpts of the Chapter 11 CPU
machinery (a miniature `gbdbg::DbgCpu`, no exercise headers), each excerpt
carrying exactly one seeded defect. All four produce plausible-looking
behavior — the way real CPU bugs hide for weeks in emulator projects.

| # | Defect | Symptom | Failing test |
|---|--------|---------|--------------|
| 1 | DAA addition path ignores H (adjusts only when low nibble > 9) | BCD sums with a half-carry come out wrong: `$45 + $38` adjusts to `$7D + $06 = $83` only by luck of the low nibble; cases like `$42+$39` (H set, low nibble valid) never adjust and yield binary garbage | `debug_daa.*` |
| 2 | CB-page flag tail forces Z=0 (base-page RLCA rule leaked in) | shifts/rotates that produce zero fail to raise Z; every `srl; jr z,...` idiom silently breaks | `debug_cb.*` |
| 3 | HALT resume adds a phantom +2 skip | after waking from HALT with IME clear, two extra instruction bytes are swallowed — loops lose iterations, marker stores never execute | `debug_halt.*` |
| 4 | taken JR cc bills only the fall-through price (forgets cycles_alt) | programs heavy on taken branches run measurably fast; cycle totals diverge from the LECTURE.md table (JR cc,e is 8 not-taken / 12 taken) | `debug_jr.*` |

## Method

1. Run `ch11_90_debug_tests` and pick ONE failing suite.
2. Trace the value flow by hand for that test's inputs (register + flag
   level). For bug 3, walk the fetch/PC accounting instruction by
   instruction; for bug 4, count M-cycles against the conditional-timing
   table in LECTURE.md.
3. Find the FIRST divergence, not the last symptom.
4. Fix, re-run, then write `bug-report.md` in this directory:

```text
bug:
root cause:
first divergence:   (exact input where stub and truth part ways)
fix:
regression test:    (name of the TEST you would add to prevent a relapse)
```

Repeat until all four suites pass. Do not fix all four blind — the point is
the isolation workflow.
