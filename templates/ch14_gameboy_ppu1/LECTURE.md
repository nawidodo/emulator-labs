# Chapter 14 — Game Boy PPU I: tiles, tilemaps and the background

The DMG Picture Processing Unit draws 160x144 pixels, refreshed ~59.7
times per second. This chapter builds the *content* half of the PPU —
how bytes in VRAM become pixels — using headless snapshot rendering.
Chapter 15 adds the *timing* half: sprites, modes and interrupts.

## Frame and scanlines

```text
frame   = 154 scanlines (70224 T-cycles)
visible = lines 0..143      -> the panel shows pixels
vblank  = lines 144..153    -> panel off; games update VRAM here
```

Each visible line is emitted left to right while the LCD controller
walks the BG surface. We model rendering as a pure function of VRAM +
LCD registers (`renderFrame`), which is both testable and exactly how
the hardware behaves when registers hold still.

## Tiles: 2bpp two-plane interleaved

A tile is 8x8 pixels in 16 bytes. Row `y` occupies bytes `2y` (plane 0,
low color-index bit) and `2y+1` (plane 1, high bit). Bit 7 is pixel
x=0:

```text
byte0 = plane0 row0     bit7 = pixel(0,0) ... bit0 = pixel(7,0)
byte1 = plane1 row0

color index = plane0_bit | plane1_bit << 1     (0..3)
```

The index is *not* a color — it selects one of four fields in a palette.

## Tilemaps

Two 32x32-tile maps live at $9800 and $9C00 (LCDC bit 3 picks the BG's;
bit 6 picks the window's). Each byte is a tile number. The surface is
256x256 pixels but the screen shows only 160x144 of it, positioned by
SCX/SCY with wrap-around modulo 256 — scrolling past an edge re-enters
the opposite side.

Tile data addressing (LCDC bit 4):

| bit4 | base | index interpretation |
|------|-------|---------------------|
| 1 | $8000 | unsigned, tile 0..255 |
| 0 | $8800 | **signed**, tile -128..127 relative to $1000 |

Signed mode lets games address nearby tiles with a single signed byte
(`ld (hl), a` with negative offsets) — forgetting it produces frames
drawn from the wrong end of VRAM.

## Palettes

BGP ($FF47) maps each 2-bit index to a shade 0..3 (light→dark):
`shade = (BGP >> (index*2)) & 3`. Games flip palettes constantly for
effects (palette cycling, fades, flashing damage), so translation is
per-pixel. Sprite palettes OBP0/OBP1 arrive in chapter 15.

Our renderer maps shades to a fixed grayscale RGBA ramp
{255,192,96,0} so golden hashes are platform-independent.

## The window

The window is a second tilemap overlay that never scrolls:

- active on line LY iff LCD on && LCDC bit5 && LY >= WY;
- covers screen x >= WX-7 (WX < 7 hides it entirely);
- its content line advances through an **internal counter** incremented
  only on lines where the window drew. Consequence: toggling the enable
  bit mid-frame skips content rows instead of restarting from WY. Many
  games exploit this for split-screen status bars.

## Rendering checklist

For every pixel of every visible scanline:

```text
surface_x = (SCX + x) & 0xFF        surface_y = (SCY + LY) & 0xFF
tile_byte = map[(surface_y/8)&31][ (surface_x/8)&31 ]
offset    = tileDataOffset(LCDC bit4, tile_byte)
index     = decodePixel(vram + offset, surface_x&7, surface_y&8...)
shade     = BGP field for index
```

Get each step independently tested before composing them — that is what
the exercise ladder does: decode a tile → map through BGP → one
scanline → one frame → window.

## References

- Pan Docs: "Graphics Background", "Tile data", "Scrolling",
  "Background palette", "Window"
  <https://gbdev.io/pandocs/>
