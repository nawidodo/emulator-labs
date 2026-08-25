# 03 — Background scanline

The BG is a 32x32-tile surface scrolled with SCX/SCY. Per screen pixel:

```text
surface_x = (SCX + x) mod 256
surface_y = (SCY + LY) mod 256
tile      = vram[mapBase + (surface_y/8)*32 + surface_x/8]
pixel     = tile[(surface_y%8)*2 .. +1] bit (7 - surface_x%8)
```

| seq | function           | contract |
|-----|--------------------|----------|
| 1   | `bgMapBase`        | LCDC bit 3 selects $9800/$9C00 |
| 2   | `mapEntry`         | wrap mapX/mapY modulo 32 |
| 3   | `tileDataOffset`   | unsigned $8000 / signed $8800 addressing |
| 4   | `renderBgScanline` | full 160-pixel line as 2-bit shades |

## Acceptance

`ch14_03_bgline_tests`: map wrap, both tile-data modes, scroll wrap,
BG-disabled → shade 0.

Pan Docs: "Scrolling", "Background palette", "LCD control".
