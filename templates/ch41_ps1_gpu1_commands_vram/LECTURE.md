# Chapter 41 — PS1 GPU I: Commands and VRAM

Primary machine reference: PSX-SPX, "GPU" chapter
(https://problemkaputt.de/psx-spx.htm). All formulas below are quoted or
derived directly from it; where our educational model deviates from real
hardware deliberately, it says so in a **Deviation** box.

## 1. Where the GPU sits

The R3000A never touches framebuffer memory through its normal bus. The GPU
owns 1 MiByte of VRAM and exposes exactly two 32-bit ports:

| Port | Name | Direction | Purpose |
|---|---|---|---|
| `1F801810` | GP0 | write | rendering + VRAM-transfer packets |
| `1F801814` | GP1 | write | display control |
| `1F801810` | GPUREAD | read | VRAM->CPU download results |
| `1F801814` | GPUSTAT | read | status register |

Everything a game draws is expressed as GP0 packets; everything about
*where on the TV* pixels appear is GP1.

## 2. VRAM organization

1024 x 512 halfwords = 1 MiB of 15-bit BGR555 pixels. Coordinates are
`(x, y)`, `x < 1024`, `y < 512`. The address counters mask X to 10 bits and
Y to 9 bits: a pixel past column 1023 lands at column 0 of the SAME row;
a row past 511 wraps to the top. There is no carry-out from X to Y nor from
Y to Y — this "independent wrap" is the single most-tested transfer detail.

VRAM holds framebuffers, texture pages and CLUTs side by side; nothing is
special-cased by hardware except when you ask texture units to read it
(ch42).

## 3. The command FIFO

GP0 words enter a 64-byte (16-word) FIFO. A packet is a one-word header —
opcode in bits 24-31 — followed by an opcode-dependent number of parameter
words. Execution starts once all parameters have arrived (polygons start
after their first three vertices, which matters only for timing).

Parameter-word counts for the families implemented in this chapter:

| opcode(s) | params | packet |
|---|---|---|
| 00h,01h,03h,1Fh,E1h..E6h | 0 | one-word commands (argument inside the header!) |
| 02h | 2 | FILL: xy, wh (color rides in the header's low 24 bits) |
| 20h..23h / 28h..2Bh | 3 / 4 | mono tri / quad |
| 24h..27h / 2Ch..2Fh | 6 / 8 | textured tri / quad (decode-only here) |
| 30h..33h / 38h..3Bh | 5 / 7 | shaded (Gouraud) tri / quad |
| 34h..37h / 3Ch..3Fh | 8 / 10 | shaded+textured (decode-only here) |
| 40h..47h / 50h..57h | 2 / 4 | mono / shaded line |
| 60h..67h | 2 | variable-size rectangle |
| 68h..6Fh, 70h..77h, 78h..7Fh | 1 | fixed 1x1 / 8x8 / 16x16 rectangle |
| 80h | 3 | VRAM->VRAM copy: src, dst, wh |
| A0h, C0h | 2 | transfer headers: coord, wh |

A wrong count desynchronises every later word, so even unimplemented
commands must decode correctly. Poly-lines (48h/58h...) end with the
termination word `55555555h` and are out of scope this chapter.

## 4. Block transfers (GP0 80h/A0h/C0h)

All three normalise sizes identically (PSX-SPX "Masking for COPY Commands"):

```
Xsiz=((Xsiz-1) AND 3FFh)+1      ; range 1..400h
Ysiz=((Ysiz-1) AND 1FFh)+1      ; range 1..200h
```

so a size field of 0 degenerates to the maximum 1024x512 — size 0 means
"maximum", never "nothing". Destination addressing wraps per pixel:
`(x+col) mod 1024`, `(y+row) mod 512`; the source stream stays linear.
The copy command latches source data (128 halfword chunks on hardware)
before writing, so overlapping copies do not smear.

Transfers honour both mask bits (E6h): destinations with bit15=1 are
write-protected while check-mask is set, and set-mask forces bit15 on
written values. Transfer coordinates are absolute VRAM addresses — neither
drawing offset nor drawing area applies (nor to FILL).

GPUREAD pops latched C0h data two halfwords at a time, little-endian; odd
sizes pad one extra zero halfword ("an extra halfword is read at the end").
GPUSTAT bit27 reports pending data.

## 5. FILL (GP0 02h)

FILL writes a constant BGR555 value using the video-ram write-same feature,
which makes it roughly 8x faster than monochrome rectangles
(~0.075 CPU clocks/pixel vs ~0.5 GPU clks/pixel for rects). Parameters are
masked and rounded, NOT clipped:

```
Xpos=(Xpos AND 3F0h)                       ; steps of 10h
Ypos=(Ypos AND 1FFh)
Xsiz=((Xsiz AND 3FFh)+0Fh) AND (NOT 0Fh)   ; round UP to steps of 10h
Ysiz=(Ysiz AND 1FFh)
Fill does NOT occur when Xsiz=0 or Ysiz=0
```

Quirks worth memorising:

* Literal `Xsiz=400h` collapses (`AND 3FFh`) to 0 → **no fill**.
* But `3F1h..3FFh` rounds up to a genuine full-width 400h fill.
* Same story vertically with literal `200h`.
* The fill colour converts 24-bit RGB -> BGR555 by dropping the low three
  bits of each channel (bit15=0).
* Fill ignores drawing area, drawing offset and BOTH mask bits.

## 6. GPUSTAT (read 1F801814h)

Power-on value: `14802000h`.

| bits | meaning | source |
|---|---|---|
| 0-3 | texture page X base (N*64) | GP0(E1h) |
| 4 | texture page Y (N*256) | GP0(E1h) |
| 5-6 | semi-transparency mode | GP0(E1h) |
| 7-8 | texture page colours | GP0(E1h) |
| 9 | dither enable | GP0(E1h) |
| 10 | interlace drawing | GP0(E1h) |
| 11 | set mask bit on draw | GP0(E6h).0 |
| 12 | draw-not-to-masked-areas | GP0(E6h).1 |
| 13 | interlace field (always 1 progressive) | — |
| 14 | reverse flag | GP1(08h).7 |
| 15 | texture page Y bit9 | GP0(E1h).11 |
| 16 | horizontal res 368 | GP1(08h).6 |
| 17-18 | horizontal res 256/320/512/640 | GP1(08h).0-1 |
| 19 | vertical 240/480 | GP1(08h).2 |
| 20 | NTSC/PAL | GP1(08h).3 |
| 21 | display depth 15/24 | GP1(08h).4 |
| 22 | vertical interlace | GP1(08h).5 |
| 23 | display off (1=black) | GP1(03h) |
| 24 | IRQ1 requested | GP0(1Fh), acked by GP1(02h) |
| 25 | DMA/data request (DRQ) | depends on bits 29-30 |
| 26 | ready for command word | FIFO not full |
| 27 | GPUREAD data available | C0h latch non-empty |
| 28 | command FIFO empty | — |
| 29-30 | DMA direction | GP1(04h) |
| 31 | interlace line parity | 0 during vblank |

DRQ (bit25) semantics depend on direction: mode 0 forces it low; mode 1
requests while the FIFO is at least half empty; mode 2 mirrors bit28
(FIFO empty); mode 3 mirrors bit27 (download data ready).

## 7. GP1 display control (accurate PSX-SPX numbering)

| op | name | effect |
|---|---|---|
| 00h | reset | flush FIFO, abort packet, restore power-on registers (NOT VRAM; leaves GP0(03h)/GP1(09h) alone) |
| 01h | reset command buffer | flush FIFO, abort current packet |
| 02h | acknowledge IRQ1 | clears GPUSTAT.24 |
| 03h | display enable | bit0: 0=on, 1=off(black); GPUSTAT.23 |
| 04h | DMA direction | bits0-1 -> GPUSTAT.29-30 |
| 05h | display area start | x bits0-9, y bits10-18 |
| 06h | horizontal display range | X1/X2, 12-bit fields, 53.2224 MHz units |
| 07h | vertical display range | Y1/Y2, 12-bit fields, scanline units |
| 08h | video mode | hres/vres/mode/depth/interlace bits -> GPUSTAT.14,16-22 |

> Note: some secondary sources renumber these ("01h reset-irq, 02h display
> enable"); PSX-SPX and the real hardware use the table above.

Unknown GP1 opcodes change nothing. Unknown GP0 opcodes decode as one-word
no-ops in our model.

## 8. Rendering attributes (E1h..E6h)

* **E1h draw mode**: texpage base, semi-transparency mode, colour depth,
  dither enable, interlace drawing. Dither affects ONLY gouraud/textured
  output; this chapter runs dither-off everywhere (ch42 covers the matrix).
* **E2h texture window**: stored raw, consumed in ch42.
* **E3h/E4h drawing area**: inclusive corners; render output clips against
  them (transfers/FILL never do).
* **E5h drawing offset**: signed 11-bit X/Y added to every render vertex
  before clipping.
* **E6h mask**: bit0 force-set bit15 on written pixels; bit1 write-protect
  destinations whose bit15 is set. Applies to rendering AND transfers, not
  FILL. Untextured pixels otherwise store bit15=0.

## 9. Rasterization rules (normative for goldens)

**Sample rule.** Pixel (px,py) is owned by its centre `(px+0.5, py+0.5)`;
we evaluate on a doubled integer lattice so centres become exact points
`(2px+1, 2py+1)`.

**Signed area & culling.** `area2 = (b-a) x (c-a)` in y-down screen space.
`area2 <= 0` draws NOTHING. Positive (clockwise on screen) faces the viewer
— matching the GTE convention that games order front-facing polygons
clockwise before sending them.

> **Deviation.** The real GPU renders both windings (culling happens GTE-
> side). We cull because it gives deterministic, teachable semantics; the
> assignment fixes this rule for goldens.

**Edge functions.** For edge `v_i->v_j`: `E(P) = dx*(Py-y_i) - dy*(Px-x_i)`
on the lattice. Interior: every `E >= 0`. Incremental walk: one pixel right
adds `-2*dy`, one row down adds `+2*dx` — pure integer arithmetic, no
floating point anywhere.

**Top-left fill convention.** A boundary centre (`E == 0`) belongs to the
primitive iff its edge is TOP (`dy == 0 && dx > 0`) or LEFT (`dy > 0`).
Two adjacent primitives traverse a shared edge in the same direction, so
they classify it identically: shared edges are drawn exactly once — no
gaps, no double-draw between separately submitted triangles.

Worked example — triangle (2,2),(6,2),(2,6): the hypotenuse from (6,2) to
(2,6) has dy>0 so its boundary centres are included; rows hold
4/3/2/1 pixels; column x=6 and row y=6 are excluded. Exactly 10 pixels.

**Quads.** Literal PSX-SPX split into triangles (v1,v2,v3)+(v2,v3,v4).

> **Deviation.** With the top-left rule this split can leave a thin sliver
> near the v1 corner undrawn for perimeter-ordered quads. We keep the
> documented split for fidelity and accept the sliver; fixtures avoid
> relying on it.

**Gouraud colour.** Weight of vertex k comes from its opposite edge:
`lambda_k = (E_k << 12) / (4 * area2)` (truncating division; all terms are
non-negative inside the triangle). Channel =
`(lambda_a*c_a + lambda_b*c_b + lambda_c*c_c + 2048) >> 12` (round half-up),
clamped to 0..255, truncated to five bits (`>> 3`) when packed to BGR555.
This is our documented 12.12-fixed-point model; the real GPU interpolates
in higher precision with dithering noise on top (ch42).

**Rectangles.** Size fields follow the COPY normalisation (`((size-1)&mask)+1`),
so size 0 draws the maximum 1024x512 — unlike FILL, where 0 draws nothing!
Drawing offset applies; pixels clip to the drawing area.

## 10. Rendering timings (approximate, PSX-SPX "GPU Rendering Timings")

Peak throughput is 66 Mpix/s for monochrome rects/polygons; textured or
Gouraud polygons drop to 33 Mpix/s (an apparent design mistake). Real-world
average with overload (cache misses, semi-transparency, short scanlines,
memory refresh) is ~6-11 Mpix/s. Values below are 33 MHz CPU clocks.

| operation | cost |
|---|---|
| FILL | ~0.075 CPU clocks/pixel (write-same feature), 8x faster than mono rects |
| rect | 5 clks/scanline + 0.50/pixel (old GPU, no semi); 6 clks/scanline + 3.75 per 16-pix chunk with semi |
| triangle precalc | 10 clks base; +90 textured; +150 gouraud (overlapped with the previous triangle's render phase) |
| mono tri render | 4.75 clks/scanline + 0.50/pixel without semi; up to 3.00 clks/pixel with semi+gouraud (old GPU) |

Timings are informational this chapter: our runner executes commands
synchronously (zero-latency model).

## 11. Deviations from hardware (summary)

1. Backface culling enforced (hardware draws both windings).
2. Semi-transparent opcode variants render opaque (blending = ch42).
3. Textured primitives decode but draw nothing (ch42).
4. Dither disabled; no dither matrix.
5. No vertex max-distance (1023/511) rejection rule.
6. Zero-latency command execution; FIFO never backs up.
7. Quad split kept literal per SPX (possible sliver, see §9).
