# 03 — STAT interrupt

STAT ($FF41) ORs four sources into one interrupt line:

| source | asserts when | enabled by |
|--------|--------------|------------|
| LYC coincidence | LY == LYC (flag = STAT bit 2) | bit 6 |
| mode 2 | OAM scan | bit 5 |
| mode 1 | vblank | bit 4 |
| mode 0 | hblank | bit 3 |

**DMG quirk:** the CPU sees only the RISING EDGE of the OR-ed line.
Sources asserting back-to-back with no low gap produce a single
interrupt.

| seq | function | contract |
|-----|----------|----------|
| 1 | `statSignal`       | OR of each source gated by its enable flag |
| 2 | `coincidenceFlag`  | plain `ly == lyc` |
| 3 | `feed`             | rising-edge detector: true exactly on false→true |

## Acceptance

`ch15_03_stat_tests`: per-source truth table, edge detector behavior,
and scripted full-frame walks producing deterministic interrupt logs
(LYC match line, vblank entry at 144, OAM source on all 144 visible
lines, combined sources).

Pan Docs: "LCD status register", "STAT interrupts".
