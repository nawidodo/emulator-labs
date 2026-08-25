# 02 — Mode timing

The PPU is a dot-driven state machine: 456 dots per line, 154 lines per
frame (70224 dots ≈ 59.7 Hz). Lines 0..143 are visible, 144..153 vblank.

**Course simplification** — real mode 3 varies between 172 and 289 dots
with sprite fetches; this model uses fixed widths:

| mode | meaning | dots |
|------|---------|------|
| 2 | OAM scan | 0..79 |
| 3 | drawing (VRAM+OAM locked) | 80..251 |
| 0 | hblank | 252..455 |
| 1 | vblank | all of lines 144..153 |

| seq | function | contract |
|-----|----------|----------|
| 1 | `modeAt`        | pure line/dot → mode lookup with the boundaries above |
| 2 | `advance`       | carry dots into LY at 456, wrap LY at 154; refresh mode |
| 3 | `vramLocked`    | mode 3 on a visible line only |
| 4 | `oamLocked`     | modes 2 AND 3 on a visible line |
| 5 | `buildModeTrace`| `"ly=<n> dot=<n> mode=<m>\n"` per (ly, mode) change over N single-dot steps |

## Trace golden

`tests/public/ch15_gameboy_ppu2/traces/mode_trace.txt` holds one full
frame of transitions (442 lines), generated twice from the reference
solution — byte-identical. The chapter runner's `--trace FILE` writes the
same format for its first rendered frame; the hidden manifest hashes that
file, so any timing slip (a threshold computed off the wrong line base,
for instance) changes the hash.

## Acceptance

`ch15_02_timing_tests`: boundary assertions at every transition dot, LY
increments exactly at 456·k, frame round-trip determinism, lock
predicates, trace format.

Pan Docs: "OAM scanning", "LCD timing".
