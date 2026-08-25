# Lecture — NES PPU II: Scrolling and Sprites

Chapter 21 rendered static scenes. Real games scroll and use sprites, and
both depend on the PPU's internal "loopy" registers being modeled EXACTLY.
This chapter is the difference between an emulator that plays *SMB* and one
that shreds its status bar.

## The loopy registers: v, t, x, w

```text
bit:  0000 0000 0000 0000
      yyyx NNYY YYYX XXXX   (v and t share this 15-bit layout)
      |||| |||| ||++------ coarse X  (bits 0-4)
      |||| ++++----------- coarse Y  (bits 5-9)
      ||++---------------- nametable (bits 10-11)
      ++------------------ fine Y   (bits 12-14)
fine X lives in its own 3-bit latch x.
w is the first/second-write toggle shared by $2005/$2006.
```

Write sequences (memorize these):

| Access | First write (w=0) | Second write (w=1) |
|---|---|---|
| `$2000` | t bits 10-11 = data bits 0-1 | — |
| `$2005` | x = data & 7; coarse X -> t 0-4; w=1 | fine Y -> t 12-14; coarse Y -> t 5-9; w=0 |
| `$2006` | t 8-13 = data & 0x3F (t bit 14 cleared); w=1 | t 0-7 = data; **v = t**; w=0 |
| `$2002` read | clears w | |

`$2007` access bumps v by 1 or 32 (PPUCTRL bit 2), wrapping at 15 bits.

## Per-dot updates during rendering

While rendering, v is not CPU-owned anymore. The pipeline performs:

- `increment_x` every 8 fetch dots (dots 8..256, 328, 336): coarse X++,
  wrapping 31→0 while flipping nametable bit 10;
- `increment_y` at dot 256: fine Y++, with the famous cascade — coarse Y
  wraps 29→0 flipping nametable bit 11, but row 31 passes through WITHOUT
  the flip (attribute table row);
- `copy_x` at dot 257 (start of every sprite tile fetch phase);
- `copy_y` on the pre-render line (261), dots 280-304, once per dot.

A raster effect is exactly a mid-frame write that races against these
updates — which is why the challenge chapter makes you snapshot v/x/w at
precise dots.

## Sprite evaluation

On each visible scanline the PPU fills a secondary OAM (8 entries) with the
sprites whose Y range covers that line (`line >= y && line < y + h`, h = 8
or 16 per PPUCTRL bit 5). Observables you must reproduce:

- only the FIRST 8 in-range sprites (OAM order) are drawn;
- a 9th sets the overflow flag ($2002 bit 5);
- sprite 0's presence enables the sprite-0 hit detector.

**The overflow quirk** (toggleable in our exercise): after finding 8
sprites, hardware keeps scanning but the address counter misbehaves — it
advances once per FOUR reads instead of per sprite — so the byte compared
against the scanline drifts through tile/attr/X bytes of neighboring
entries. Result: false positives (Tetris!) and missed detections. We model
this deterministically; the clean variant exists so tests can pin both.

## Sprite 0 hit — exact conditions

Sprite 0's opaque pixel overlapping an OPAQUE background pixel sets
$2002 bit 6, EXCEPT:

- pixel x = 255 never hits;
- pixels x < 8 don't hit when EITHER left-column clip is active
  (PPUMASK bit 1 hides bg, bit 2 hides sprites);
- no hit when bg or sprite rendering is off (PPUMASK bits 3/4).

Games poll this flag for split-screen scrolling; getting any clause wrong
breaks a whole genre of effects.

## Priority

Per pixel:

1. Among evaluated sprites covering the pixel, the LOWEST OAM index with a
   non-transparent pixel provides color AND its attribute bit 5 decides
   front/back — even if a higher-index sprite would have been in front.
2. A behind-sprite loses to an opaque bg pixel; transparent bg (tile color
   0) always lets sprites show.
3. Left-column clips hide units entirely before priority applies.

## Debugging by screenshot diff

The coding test builds a diagnoser: hash two raw frames, find the first
divergent pixel, and classify whether all differences are explained by a
one-pixel horizontal shift (broken fine-X latch) or a one-scanline vertical
shift (broken coarse/fine-Y latch). This mirrors how real emulator
developers bisect PPU bugs against reference screenshots.

Reference: NESdev Wiki "PPU scrolling", "PPU sprite evaluation",
"PPU sprite priority".
