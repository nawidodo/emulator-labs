# Chapter 32 — SNES Bus and PPU

The Super Nintendo is the first machine in this course where "the video
chip" is really a *system*: a banked 24-bit address space shared between
the CPU and a PPU that owns its own private memories (VRAM, CGRAM, OAM)
plus a fixed-function raster pipeline with per-layer priority, windowing
and color arithmetic. This chapter builds that system bottom-up.

## Study

```text
banked memory map
WRAM
VRAM
CGRAM
OAM
background modes
Mode 7
windows
color math
```

## 1. The banked map

A 65C816 can only emit 16-bit addresses, so the SNES adds an 8-bit bank
register: addresses are 24 bits, `bank << 16 | offset`. The decode is
*not* linear:

| region      | addresses                                   | note                          |
|-------------|---------------------------------------------|-------------------------------|
| WRAM direct | `$7E0000-$7FFFFF`                           | two full 64 KiB banks         |
| WRAM mirror | banks `$00-$3F`, offsets `$0000-$1FFF`      | aliases the LOW 8 KiB of WRAM |
| PPU ports   | banks `$00-$3F`, offsets `$2100-$21FF`      | same page in every system bank|
| Cart ROM    | banks `$00-$3F` (+ `$80-$BF`), `$8000-$FFFF`| LoROM: 32 KiB pages           |

Two consequences worth internalizing:

* **Mirroring is decoding, not copying.** Writing through `$000123`
  changes the byte seen at `$7E0123` because both decode to the same
  WRAM cell — no second copy exists.
* **LoROM packs 32 KiB pages**: `rom_index = (bank & $3F) << 15 |
  (offset & $7FFF)`. Offset bit 15 selects the page half, which is why
  code and data live in the upper half of each bank on real carts.

Access speed differs by region. The fastROM bit (`$420D`) halves the wait
states of ROM *only in the mirrored banks `$80-$BF`* — 8 master cycles
become 6. WRAM and PPU ports stay slow.

## 2. PPU memories

* **VRAM** — 64 KiB, addressed as 32 K *words*. Tiles are built from
  planar bit-packed words, so word addressing is what the fetchers use;
  the CPU streams bytes through `$2118/$2119` (low/high halves).
* **CGRAM** — 256 BGR555 entries. Each channel is 5 bits; expansion to a
  modern framebuffer replicates the top bits: `r8 = (r5 << 3) | (r5 >>
  2)`. That maps channel `$1F` to `$FF` exactly.
* **OAM** — sprite attributes split across a low table (4 bytes per
  sprite) and a tiny high table (2 bits per sprite: x msb and size
  select). An accessor must combine both tables. Real hardware has 128
  sprites; this chapter models the 512-sprite layout the curriculum
  specifies.

## 3. Background modes 0 and 1

Every BG layer repeats the same fetch chain per pixel:

```text
scroll -> tilemap entry -> flip -> planar tile pixel -> palette band -> candidate
```

* Tilemap entries are 16 bits: `tile(0-9) palette(10-12) priority(13)
  hflip(14) vflip(15)`.
* 2bpp tiles are 8 bytes (two planes); 4bpp tiles are 32 bytes (planes
  0/1 interleaved per row in the first half, planes 2/3 in the second).
* Mode 1 = two 4bpp layers + one 2bpp layer. Mode 0 = four 2bpp layers,
  each owning its own palette band.

Composition is a priority problem. Our documented rule (see exercise 03):
each opaque candidate carries key `layer * 2 + (priority ? 0 : 1)` and the
smallest key wins — BG1 outranks BG2 outranks BG3 regardless of priority
bits; within a layer, priority=1 beats priority=0. Sprites are omitted.

## 4. Windows and color math

A window rectangle (`left..right`, inclusive, invertible) gates layers:
a layer shows only where the pixel lies inside the effective window AND
its mask bit allows it. Color math then combines the surviving color with
the backdrop in the 5-bit domain — add or subtract, optionally halved,
saturating at 0..31. Note the halving applies to the *intermediate*
sum/difference.

## 5. Mode 7

One 1024x1024 layer of raw color bytes, warped per pixel by an 8.8
fixed-point matrix:

```text
u = A*(x - x0) + B*(y - y0) + (hofs << 8)
v = C*(x - x0) + D*(y - y0) + (vofs << 8)
px = u >> 8   (arithmetic shift = floor!)
```

Out-of-range handling has four hardware behaviors; we keep two documented
ones: backdrop-fill (hardware modes 0/1/2 simplified into one) and wrap
(hardware mode 3). Watch the floor semantics for negative coordinates —
truncating instead of flooring produces a one-pixel seam along every tile
boundary.

## Exercises

Implement the bus map, PPU memories, mode 0/1 rendering with windows and
color math, then Mode 7. Debug a seeded rendering-order bug (90_debug).
Render a supplied scene state for the challenge (91_challenge), and finish
with an unseen-spec coding test (99_coding_test).

## References

* Anomie's SNES Hardware Manual — PPU sections (`fullsnes` also works).
* SNES Development Manual (Nintendo, 1993) — memory map and timing.
