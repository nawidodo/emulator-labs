# 01 — OAM sprites

OAM ($FE00) holds 40 entries of 4 bytes — `y, x, tile, flags`:

| field | meaning |
|-------|---------|
| y, x  | position with a 16/8-pixel offset: sprite covers lines `[y-16, y-16+h)` and columns `[x-8, x)` |
| tile  | tile number; in 8x16 mode the LSB selects the half (`tile&0xFE` top, `\|1` bottom) |
| flags | 0x80 BG-over-sprite priority, 0x40 Y flip, 0x20 X flip, 0x10 palette (OBP1) |

`x == 0` hides a sprite entirely. **Course choice:** such entries are
skipped *before* the 10-per-line limit — they never consume a slot.

| seq | function | contract |
|-----|----------|----------|
| 1 | `spriteHeight`   | LCDC bit 2: 16 when set, else 8 |
| 2 | `collectSpritesForLine` | first ≤10 covering entries in OAM order; x==0 skipped pre-limit; stop at 10 |
| 3 | `renderSpritesScanline` | per-column winner (smaller x, tie → lower OAM index), transparency at index 0, BG-priority vs BG color INDEX, flips, 8x16 pairing, OBP0/OBP1 |

## Acceptance

`ch15_01_oam_tests`: limit semantics, x==0 handling, transparency,
priority flag on zero/nonzero BG indices, both flips, 8x16 half pairing,
palette select, x-then-OAM-order priority.

Pan Docs: "OAM", "Sprite priorities", "Sprite palettes".
