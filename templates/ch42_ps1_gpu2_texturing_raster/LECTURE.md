# LECTURE — GPU II: Texturing & Rasterization Accuracy

Primary reference: PSX-SPX (nocash), "Graphics Processing Unit (GPU)" —
Render commands, Rendering Attributes, Video Memory (VRAM), Texture Caching,
and the 24bit→15bit dithering section. This chapter turns last chapter's
flat primitives into **textured** ones without losing a single pixel of
determinism.

---

## 1. Texture pages

A texture page is a 256×256-texel region of VRAM. Its base address is
coarse-grained:

| Field | Source | Meaning |
|---|---|---|
| page X base | GP0(E1h) bits 0-3 (or Texpage attribute) | N × 64 halfwords |
| page Y base | bit 4 of the same fields | 0 or 256 lines |

The same field values appear in GPUSTAT bits 0-8, which is why games can
poll the status register to confirm the active texpage.

One page holds 256×256 texels *regardless* of colour depth; what changes is
the packing:

| Depth | Texels/halfword | Page row width | Address of texel (u,v) |
|---|---|---|---|
| 4bpp CLUT | 4 nibble lanes | 64 halfwords | `base + v*1024 + u/4`, lane `u&3` |
| 8bpp CLUT | 2 byte lanes | 128 halfwords | `base + v*1024 + u/2`, lane `u&1` |
| 15bpp direct | 1 | 256 halfwords | `base + v*1024 + u` |

Note `v*1024`: successive page rows live on successive VRAM **lines** — the
page row width is *not* the VRAM stride. Lane 0 is always the leftmost
texel (lowest nibble / low byte).

**Odd-page-X lane mirror (hardware quirk).** When the raw page-X *field* is
odd, hardware mirrors the lane order inside every texture halfword:
4bpp reads nibble `3-(u&3)`, 8bpp reads the opposite byte. The reference
implementation reproduces this; 90_debug shows the scramble that appears
when it is missing.

### Colour lookup tables

The fetched index selects a halfword in a CLUT row located by the primitive's
CLUT attribute (carried in the upper half of texcoord words):

```
bits 16-21 of word : CLUT X base, in 16-halfword steps (so X = field*16)
bits 22-30         : CLUT Y, 0..511 (9 bits)
```

4bpp uses a 16×1 CLUT row, 8bpp a 256×1 row. The GPU caches CLUTs on-chip
(512 B: one 16-halfword line shared by both depths + 240 halfwords for
8bpp indices ≥ 0x10); any textured command with a changed CLUT address
reloads it even if nothing is drawn.

### Texture window (GP0(E2h))

```
Texcoord = (Texcoord AND NOT(Mask*8)) OR ((Offset AND Mask)*8)
u clipped to 8 bits, v to 9 bits afterwards
```

Mask/Offset are raw 5-bit fields counted in 8-texel steps. Mask field 1 masks
bit 3 → the pattern repeats every 16 texels; mask 3 → 32; mask 7 → 64.
The data is not replicated in VRAM; the GPU just reads the wrapped
coordinates as if it were.

---

## 2. Texel fetch pipeline (stage view)

For each covered pixel, in order:

1. **primitive_setup** — add the GP0(E5h) drawing offset to vertex X/Y.
   The offset is applied BEFORE clipping: it can push a primitive into or
   out of the drawing area entirely.
2. **rasterize** — walk covered pixels (edge-function test with top-left
   fill rule for polygons; rectangles include their lower-right pixel).
   UVs interpolate screen-linearly in 8.8 fixed point — the PSX has no
   perspective correction, so textures warp on tilted polygons.
3. **texture_fetch** — window-wrap (u,v), select the halfword, extract the
   lane (with odd-page mirror), look up CLUT if depth < 15bpp.
4. **blend_pixel** — modulate/decal, optional dither, optional semi-
   transparent blend against the backdrop, then the write epilogue applies
   the mask policy.

## 3. Blending modes

Command word bit 24 ("raw-texture") picks between two shading paths; bit 25
enables semi-transparency on top of either:

| Mode | Formula (per component) |
|---|---|
| Modulation (bit24=0) | `out8 = min(255, (texel8 * shade8) >> 7)` |
| Decal / raw (bit24=1) | `out = texel`, colour word ignored |

Details that matter for exactness:

- `texel8` expands the 5-bit texel component by replication: `(c<<3)|(c>>2)`
  so 31 → 255.
- The shade comes from the primitive colour word as full 8-bit values;
  **80h is unity** (`(255*128)>>7 = 255`). Shades above 80h overdrive and
  saturate at 255 — this is how games brighten textures beyond their stored
  range.
- Truncation back to 5 bits (`>>3`) happens only at the very end.

### Semi-transparency equations

Selected by Texpage bits 5-6, applied per 5-bit component with saturation
(B = backdrop read from VRAM, F = front pixel):

| mode | equation |
|---|---|
| 0 | `B/2 + F/2` |
| 1 | `B + F` |
| 2 | `B - F` |
| 3 | `B + F/4` |

Semi-transparent rendering forces a read-modify-write per pixel, which is
also visible in timing (see §6).

### Transparency processing rules

- A texel whose RGB bits are ALL zero (`x000h`) is **fully transparent**
  and skipped — for opaque and semi-transparent commands alike. This is why
  PSX textures cannot contain true black.
- Unless the STP flag (bit15) is set: `8000h` is drawable black — opaque
  black on opaque commands, blended-as-black on semi-transparent commands.
- Write epilogue: when Set-mask (E6h bit0) is off, written bit15 equals the
  texture's own STP flag (preserved through decal; untextured writes get 0).
  When Set-mask is on, written bit15 is forced to 1.

## 4. Dithering

Enabled by Texpage bit 9. The 4×4 offset table from PSX-SPX:

```
-4  +0  -3  +1
+2  -2  +3  -1
-3  +1  -4  +0
+3  -1  +2  -2        index = kDither[y & 3][x & 3]
```

Offsets are added to the **8-bit** R/G/B values after modulation; the sum is
saturated to 00h..FFh and only then divided by 8 into the final 5-bit
component. Placement rules (all reproduced by this chapter):

- POLYGONs dither only with Gouraud shading or texture blending (modulate);
  decal polys never.
- LINEs always dither (out of scope here).
- RECTs are NEVER dithered.

## 5. Drawing area, offset, and mask bits

**Drawing area** — GP0(E3h)/(E4h), X in bits 0-9, Y in bits 10-19.
The bounds are INCLUSIVE at both ends: `X1 <= x <= X2 && Y1 <= y <= Y2`.
An off-by-one here silently drops the last column/row (see 90_debug bug B).

**Drawing offset** — GP0(E5h), signed 11-bit X (bits 0-10) and Y (bits 11-21).
Added to every rendered vertex coordinate BEFORE the clip test.

**Mask bits** — GP0(E6h):

| bit | name | effect |
|---|---|---|
| 0 | Set-mask | force bit15 on written pixels (else bit15 = texture STP flag) |
| 1 | Check-mask | skip writes onto destinations whose bit15 is set |

Mask affects rendering commands AND CPU→VRAM transfers (per halfword), but
NOT the Fill-VRAM command.

## 6. Timing flavour

Per-pixel costs that matter when you wonder why games used rects for HUDs
(numbers from PSX-SPX GPU Rendering Timings, old GPU):

| Case | Cost per span |
|---|---|
| Fill VRAM | ~0.075 clk/pixel (write-combined) |
| Monochrome rect | ~8× fill cost |
| 15bpp texture, no semi | 8.25 clks/pixel |
| 4/8bpp texture, no semi | 10 clks/pixel (+CLUT reload: 16 clks for 16 colours, 256 clks for 256) |
| any texture, semi-transparent | 11 clks/pixel (read-modify-write) |

Texture cache: 2 KB, 256 lines × 4 halfwords; misses cost ~2 clk/halfword.
Changing TexPage base or executing any COPY command flushes it; FILL does not.

## 7. Worked example — one pixel end to end

Setup: E1h pxf=0 depth=4bpp, CLUT at (0,240) with entry i = `0x111*(i+1)`,
packed texture row 0 halfword 1 = `0x7654`. Primitive: opaque modulated rect
shade `(128,128,128)`, no dither, pixel at screen (44,12), uv=(4,2).

1. fetch: halfword at `0 + 2*1024 + 4/4` = line 2, x 1 → `0x7654`;
   lane `4&3 = 0` holds nibble `4`. Entry 4 → colour `0x555`.
2. modulate: shade 128 ≡ unity → stays `0x555` at 8-bit precision.
3. no semi, no dither → blend output `0x555`.
4. mask off → written bit15 = texel STP = 0. VRAM(44,12) = `0555`.

Change the E1h page field to 1 (odd) and everything shifts: lane mirror
reads nibble `3-(4&3)=3` of the same halfword → entry 7 → `0x888`.

## 8. Study checklist

- [ ] Decode GP0(E1h)/Texpage attribute fields blind.
- [ ] Compute the halfword address of an arbitrary (u,v) in all three depths.
- [ ] Reproduce the four semi-transparency equations with saturation.
- [ ] Explain why `80h` shade means "unchanged" but `FFh` does not mean double.
- [ ] Predict which pixels survive clip+offset combinations, inclusive edges.
- [ ] Know which commands dither, which honour mask bits, which ignore the
      drawing area (Fill!).
