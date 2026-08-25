# 01 — Tile decode

The Game Boy tile is the atom of all background, window and sprite
graphics: 8x8 pixels stored as **2bpp two-plane interleaved** rows.

```text
byte 2y+0 : plane 0 (color-index bit 0) for row y
byte 2y+1 : plane 1 (color-index bit 1) for row y
```

Within each byte bit 7 is pixel x=0. Combining both planes yields a
2-bit *color index* (0..3) which is later mapped through a palette —
this exercise stops at raw indices.

## Task

Implement in `tile.hpp`:

| seq | function     | contract |
|-----|--------------|----------|
| 1   | `planeBit`   | bit `(7 - x)` of a plane byte |
| 2   | `tilePixel`  | `plane0_bit \| plane1_bit << 1` |
| 3   | `decodeTile` | full 8x8 into row-major indices |

## Acceptance

`ch14_01_tile_tests` passes. The fixture bytes mirror the committed
`tests/public/ch14_gameboy_ppu1/fixtures/tile_arrow.bin`.

Pan Docs reference: "Tile data" section,
<https://gbdev.io/pandocs/Tile_Data.html>.
