# Chapter 28 — GBA PPU: Bitmap Modes, Tiles, Affine, Sprites, Compositing

Primary reference: [GBATEK — LCD Video Controller](https://problemkaputt.de/gbatek.htm#lcdvideocontroller).
Test material: [mGBA suite](https://github.com/mgba-io/suite) (optional,
student-supplied ROM — never committed here).

The GBA PPU is a scanline renderer: every 1232 cycles it produces one row of
240 pixels, 228 rows per frame (160 visible + 68 VBlank). Unlike later
consoles there is no floating point anywhere — every transform is integer,
usually **8.8 fixed point**. This chapter builds an accurate-by-construction
software scanline compositor, one feature group at a time.

## Video memory map

```text
04000000  DISPCNT       display control
04000008  BG0CNT..BG3CNT     background control (BGn at +8+2*n)
04000010  BG0HOFS/VOFS       text BG scroll (BGn pair at +0x10+4*n)
04000020  BG2PA/PB/PC/PD     affine parameters (BG3 set at +0x30)
04000028  BG2X/BG2Y          affine reference point (BG3 at +0x38)
04000040  WIN0H/V, WIN1H/V   window rectangles
04000048  WININ/WINOUT       window layer enables
0400004C  MOSAIC             block size W/H for BG and OBJ
04000050  BLDCNT             blend control (targets + mode)
04000052  BLDALPHA           EVA/EVB coefficients
04000054  BLDY               brightness coefficient
05000000  Palette RAM   1 KiB (512 u16 colors: BG 0-255, OBJ 256-511)
06000000  VRAM          96 KiB (bitmap pages, tiles, maps)
07000000  OAM           1 KiB (128 sprites x 3 u16 attrs + rotation data)
```

All video RAM is 16-bit wide. 8-bit formats (mode 4 pixels, 8bpp tiles) pack
two pixels per u16; random byte writes are impossible on hardware and our
`wr16` model reproduces that honestly.

## DISPCNT

```text
bit  0-2  Video mode 0-5 (6,7 invalid)
bit  4    Page select (modes 4/5): 0 = front frame at 06000000
bit  5    OBJ tiles in 1D mapping (else 2D)
bit  6    Forced blank (screen white, VRAM freely writable)
bit  7    OBJ enable
bit  8-11 BG0..BG3 enable (which BGs exist depends on mode)
bit 13/14 Window 0 / Window 1 enable, bit 15 OBJ window enable
```

Background availability per mode:

| Mode | BG0 | BG1 | BG2 | BG3 | Notes |
|------|-----|-----|-----|-----|-------|
| 0    | txt | txt | txt | txt | pure tiled |
| 1    | txt | txt | aff | —   | two text + one affine |
| 2    | —   | —   | aff | aff | two affine |
| 3    | —   | —   | bmp | —   | 240x160 15bpp direct |
| 4    | —   | —   | bmp | —   | 240x160 8bpp paletted, double buffer |
| 5    | —   | —   | bmp | —   | 160x128 15bpp, double buffer |

## Bitmap modes

* **Mode 3**: each u16 at `06000000 + (y*240+x)*2` is one BGR555 color:
  `0bbbbbgggggrrrrr`. There are no transparent colors; the bitmap always wins
  its priority level.
* **Mode 4**: bytes index the 256-entry BG palette. Two pages live at
  `06000000` and `0600A000`; DISPCNT bit 4 selects which one the PPU reads so
  the game can draw into the other page and flip on VBlank.
* **Mode 5**: like mode 3 but only 160x128 pixels, still two pages at
  `06000000` / `0600A000`. The hardware scales nothing — the small image sits
  in the top-left corner and the rest of the line shows the backdrop.

Color conversion to host RGBA8888 uses *bit replication*
`v8 = (v5 << 3) | (v5 >> 2)` so that 31 maps to exactly 255 with no rounding
table.

## Text backgrounds (modes 0/1)

Each enabled text BG has:

* **BGnCNT**: priority (bits 0-1), character base block (bits 2-3, 16 KiB
  units), mosaic enable (6), 8bpp flag (7), screen base block (bits 8-12,
  2 KiB units), size (14-15).
* Text BG sizes: 256x256, 512x256, 256x512, 512x512 pixels.
* **Screen entry** (u16): tile number bits 0-9, hflip bit 10, vflip bit 11,
  palette bank bits 12-15.
* Tiles are 8x8. A 4bpp tile is 32 bytes: row-major, low nibble = left pixel.
  Index 0 is transparent. In 8bpp tiles (64 bytes) the bank field is ignored.
* Scroll registers BGnHOFS/BGnVOFS are masked to 9 bits — they wrap at 512,
  which is why a scroll of 513 behaves like 1.

Priority decides layering between backgrounds; equal priority means the lower
BG number wins against another BG, but a sprite beats any background at the
same priority.

## Affine backgrounds and the transform math

Affine BGs (BG2 in mode 1, BG2/BG3 in mode 2) map a square texture of
128/256/512/1024 texels onto the screen through an inverse matrix:

```text
PA PB   s16 fixed 8.8        texture_x = PA*(x - dx) + PB*(y - dy) >> 8
PC PD                        screen_y analogous with PC/PD
```

`dx/dy` (BGnX/BGnY) is the *texture coordinate of screen pixel (0,0)*. The
hardware keeps **internal latched counters**: at scanline 0 it computes

```text
internal_x = dx; internal_y = dy
```

then per pixel adds PA/PC, and at every following scanline re-latches from
the written reference plus `PB*line >> 8`, `PD*line >> 8`. The internal
counters wrap: texel fetch masks them by the texture size (`& (size-1)`),
which is why affine backgrounds repeat seamlessly forever. Emulators that let
the counters overflow into the sign bit instead produce the classic "affine
background tears after 128 lines" bug.

## Sprites (OAM)

Per sprite three u16 attributes plus an optional rotation matrix:

```text
ATTR0: y (0-255), rotation/scale flag (8), double-size/disable (9),
       mode (10-11: normal/semi/wnd), shape (14-15: square/wide/tall)
ATTR1: x (0-511, 9 bits), matrix select (9-12, affine),
       hflip (12)/vflip (13, non-affine), size (14-15)
ATTR2: tile (0-9), priority (10-11), palette bank (12-15)
```

Size table (square/wide/tall x size bits): 8/16/32/64 base sizes scaled by
shape. Tile addressing differs for 1D vs 2D mapping (DISPCNT bit 5): 1D packs
consecutive rows linearly, 2D uses 32-tile rows — games pick whichever makes
their animation frames contiguous.

**Affine sprites** reuse the same 8.8 math as affine BGs: coordinates are
first centered (`x - w/2`, `y - h/2`), mapped through the matrix, then
re-centered; "double size" renders around a 2x range with half-size offsets.

Priorities: lower value wins; ties between sprite and BG go **to the sprite**;
ties between two sprites go to the lower OAM index. Transparent pixels never
occlude anything.

## Windows

WIN0H/WIN0V and WIN1 define rectangles; WININ/WINOUT give each window (and
the area outside both) a 6-bit mask of visible layers: BG0-BG3, OBJ, and
"blend enable". A pixel inside window 0 uses window 0's mask, else window 1's,
else the OUT mask. The OBJ window (sprite-shaped mask) follows the same idea
but is documented rather than implemented in these exercises.

## Mosaic

MOSAIC packs four nibbles: BG width, BG height, OBJ width, OBJ height
(0 = disabled). Effectively the scroll offsets used for rendering are
quantized to blocks of `1+n` pixels: all pixels in a block sample the source
at the block's origin. Implemented here for background layers; sprite mosaic
is documented.

## Blending

BLDCNT selects first-target layers (bits 0-5), effect mode (6-7):
0 none, 1 alpha, 2 lighten, 3 darken — plus second-target layers (8-13).
BLDALPHA holds EVA/EVB (5-bit weights, values above 16 saturate). Alpha blend:

```text
out = min(31, (top*EVA + bottom*EVB) >> 4)
```

applied only where the top pixel belongs to a first target AND the pixel below
belongs to a second target; otherwise the top color passes through. Lighten/
darken apply `BLDY`: toward white/black by `c + (31-c)*BLDY/16` and
`c - c*BLDY/16`. Semi-transparent sprites force themselves to be a first
alpha target with the backdrop as second target.

## Scanline composition order

For each visible line the compositor must resolve, per pixel: which layers
produce a non-transparent pixel, ordered by (priority, layer class, index);
apply window masks; pick the top layer; if blending applies, also fetch the
second layer below it. Doing this bottom-up per scanline (instead of
per-pixel full sort) is what real renderers and mGBA do, and it keeps the
cost proportional to layers, not pixels.

## Testing strategy

Everything here is deterministic: no wall clock, no RNG. Golden frame hashes
(FNV-1a 64 over raw RGBA8888 bytes) are generated by the reference solution
and committed under `tests/public/ch28_gba_ppu/frames/` with provenance notes.
External suites such as the mGBA graphics tests gate behind
`requires_rom`/`optional` manifest entries because their ROM images can never
be committed to this repository.
