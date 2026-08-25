# ch15 fixture provenance

All fixtures are synthetic, generated deterministically by
`tools/make_snapshots.py` (this directory) — no RNG, no wall time, no
ROM content from any commercial product. The script writes BOTH the
public snapshots and the hidden fixtures.

```bash
python3 tests/public/ch15_gameboy_ppu2/tools/make_snapshots.py <repo-root>
```

## Files

| file | size | coverage |
|------|------|----------|
| `snapshots/sprites_limit.ppu2` | 8362 B | 14 sprites covering one line: first-ten-in-OAM-order limit beats smaller x; two x==0 trap entries |
| `snapshots/sprites_tall_flip.ppu2` | 8362 B | LCDC 8x16 mode, X/Y flips, OBP1 select, BG-priority flag over a checker BG |
| `snapshots/combo_scroll.ppu2` | 8362 B | SCX=103 SCY=57 wrap, window on $9C00 (WY=96 WX=7), sprites incl. one priority-hidden |
| `snapshots/combo_window.ppu2` | 8362 B | window on $9C00 from WY=0, white BG — target for `--window-off-lines 32:96` |
| `snapshots/combo_signed.ppu2` | 8362 B | $8800 signed tile bytes on BOTH maps, mixed palettes/flips/priority sprites |
| `../hidden/ch15_gameboy_ppu2/fixtures/h1_sprite_heavy.ppu2` | 8362 B | dense sprite field: limit order, x==0 traps, priority, OBP1, X flips over checker BG |
| `../hidden/ch15_gameboy_ppu2/fixtures/h2_trace_scene.ppu2` | 8362 B | scrolled scene with window; drives the `--trace` hash case |
| `../hidden/ch15_gameboy_ppu2/fixtures/h3_window_toggle.ppu2` | 8362 B | banded window on $9C00 (WY=8); rendered with `--window-off-lines 32:96` |

## Snapshot v2 layout ("PPU state file v2", little-endian)

```text
0x0000  0x2000  VRAM ($8000-$9FFF)
0x2000  0x00A0  OAM   (40 entries x 4 bytes: y, x, tile, flags)
0x20A0  LCDC    0x20A1 STAT   0x20A2 BGP   0x20A3 OBP0  0x20A4 OBP1
0x20A5  SCY     0x20A6 SCX    0x20A7 WY    0x20A8 WX    0x20A9 LYC
```

Total 0x20AA = 8362 bytes.

## Listings

Tile plane bytes are emitted by the generator's `tile_bytes()` family
(`solid`, `even_stripes`, `diag`, `corner`, `checker`) from explicit
index grids; see the script for each scene's map fill rules.
Regenerating reproduces byte-identical files:

| file | FNV64 |
|------|-------|
| snapshots/sprites_limit.ppu2 | 4F3B1BF9CD75E373 |
| snapshots/sprites_tall_flip.ppu2 | 1046E6BBCADD9601 |
| snapshots/combo_scroll.ppu2 | 2081336036C6B43C |
| snapshots/combo_window.ppu2 | 7C81D99CD2105164 |
| snapshots/combo_signed.ppu2 | F0EE3564BF760D81 |
| h1_sprite_heavy.ppu2 | B045A52FC330BCCF |
| h2_trace_scene.ppu2 | D9AA5B8607167E15 |
| h3_window_toggle.ppu2 | 0CD156D4E4FBF8C2 |

Golden frame/trace hashes derived from these snapshots live in
`goldens/goldens.md`, `traces/mode_trace.txt` and
`tests/hidden/ch15_gameboy_ppu2/manifest.json`; they were produced by
the reference solution runner (run twice per scene, byte-identical).
