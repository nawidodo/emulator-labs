# Chapter 15 — Game Boy PPU II: sprites, modes and STAT interrupts

Chapter 14 built the *content* half of the PPU — bytes to pixels. This
chapter builds the *timing* half: how the LCD controller walks its 154
lines, how sprites enter the picture, and how the PPU tells the CPU what
it is doing through STAT.

## Sprites: OAM

OAM ($FE00-$FE9F) holds 40 entries of 4 bytes:

```text
byte 0: y   byte 1: x   byte 2: tile   byte 3: flags
flags: 0x80 BG priority | 0x40 Y flip | 0x20 X flip | 0x10 palette (OBP1)
```

Sprite positions carry a 16/8-pixel offset: a sprite at (y,x) covers
screen lines `[y-16, y-16+h)` and columns `[x-8, x)`. `y=0`/`x=0` push a
sprite entirely off-screen — `x==0` hides it, and the hardware skips such
entries *before* applying the per-line sprite limit.

Hardware rules our renderer models exactly:

- **10-sprite limit**: only the FIRST ten entries covering a line are
  evaluated, in OAM order — a better-positioned sprite later in OAM still
  loses its slot.
- **Priority**: for each pixel the winner is the sprite with smaller X;
  ties go to the lower OAM index.
- **Transparency**: tile color index 0 never draws, whatever the palette.
- **BG-over-sprite flag** (0x80): where the final BG/window COLOR INDEX
  is nonzero, the sprite pixel is suppressed. The index — not the shade —
  decides.
- **Flips**: X flip reverses bit order inside the tile row; Y flip picks
  row `height-1-y`.
- **8x16 mode** (LCDC bit 2): tile LSB selects the half (`tile&0xFE` top,
  `|1` bottom); the LSB must be 0 in OAM.
- **Palettes**: OBP0/OBP1 map indices 1..3 to shades exactly like BGP.

## Timing: dots, lines, modes

The PPU clock ticks in dots: 456 per line, 154 lines per frame = 70224
dots ≈ 59.7 Hz. Lines 0..143 are visible; 144..153 are vblank. Each
visible line passes through three modes:

```text
mode 2  OAM scan   dots   0..79    OAM locked
mode 3  drawing    dots  80..251   OAM + VRAM locked
mode 0  hblank     dots 252..455
mode 1  vblank     all of lines 144..153
```

Real mode 3 length varies (172..289 dots) with sprite fetches; this
course uses fixed widths so the model stays pure and hashable. LY
increments at every 456-dot boundary and wraps 153 → 0. VRAM/OAM access
locks follow the mode — that is why games only update VRAM during hblank
or vblank, and why getting this wrong corrupts real displays.

## STAT: telling the CPU

STAT ($FF41) ORs four sources into one interrupt line:

| source | asserts when | enable |
|--------|--------------|--------|
| LYC | LY == LYC | bit 6 |
| mode 2 | OAM scan starts | bit 5 |
| mode 1 | vblank starts | bit 4 |
| mode 0 | hblank starts | bit 3 |

DMG quirk: the CPU interrupts on the **rising edge of the OR-ed line**,
not per source. Two sources back-to-back with no low gap produce one
interrupt — model it with an edge detector fed once per sample.

## Window counter recap

The window's content advances through an internal line counter that
increments ONLY on lines where the window drew (chapter 14). Toggling the
window enable mid-frame therefore SKIPS content rows instead of
restarting from WY — the runner's `--window-off-lines A:B` extension lets
you hash that behavior directly.

## Rendering checklist (full pipeline)

```text
for ly in 0..143:
  BG pass      surface_x=(SCX+x)&255 ... shade = BGP[index]
  window pass  if active(ly): overwrite x >= WX-7 with map[windowLine]
               advance windowLine only if it drew
  sprite pass  collect <=10 covering OAM entries -> per-column winner ->
               transparency/priority -> OBP shade
```

Test each stage independently before composing them — that is what the
exercise ladder does: OAM scan → timing machine → STAT edges → full frame.

## References

- Pan Docs: "OAM", "Sprite priorities", "LCD timing",
  "LCD status register"
  <https://gbdev.io/pandocs/>
