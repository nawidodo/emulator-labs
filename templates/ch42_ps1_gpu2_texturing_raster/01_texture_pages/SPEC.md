# SPEC — 01_texture_pages

Scope: texture page addressing, texture-window wrapping and texel fetch for
the three colour depths. Blending is intentionally out of scope (exercise 02);
this exercise's fixtures draw **raw-texture** rectangles (`GP0(65h)`) so the
fetched texel lands in VRAM unmodified.

## Pipeline stages (visible separation)

```
primitive_setup   shared/gpu_device.hpp (drawing offset added here)
rasterize         shared/gpu_device.hpp (rect blit / edge-function walk)
texture_fetch     tex_stages.hpp   <- this exercise (marked blocks)
blend_pixel       tex_stages.hpp   (pass-through: raw texture)
```

## Texture pages

A page holds 256x256 texels; its base address is
`page_x_field * 64` halfwords, `page_y_base` lines, where `page_x_field`
is the raw 4-bit field of GP0(E1h) and `page_y_base` is bit4 of the same
register (0 or 256).

| Depth | Texels/halfword | Halfwords per page row | Address of texel (u,v) |
|---|---|---|---|
| 4bpp  | 4 (nibble lanes, lane 0 = leftmost) | 64  | `base + v*64 + u/2/2`, nibble `u&3` |
| 8bpp  | 2 (byte lanes, low byte = leftmost) | 128 | `base + v*128 + u/2`, byte `u&1` |
| 15bpp | 1 (direct colour)                   | 256 | `base + v*256 + u` |

CLUT lookup: the fetched index selects a halfword at
`(clut.x + index, clut.y)`; the CLUT base comes from the primitive's CLUT
attribute (X in 16-halfword steps, Y direct).

### Odd-page-X lane flip (hardware quirk)

When the raw `page_x_field` is **odd**, the real GPU mirrors the lane order
inside each halfword: 4bpp reads nibble `3-(u&3)`, 8bpp reads the opposite
byte. The reference implementation reproduces this; see 90_debug for the
bug that appears when it is missing.

## Texture window (GP0(E2h))

```
Texcoord = (Texcoord AND NOT(Mask*8)) OR ((Offset AND Mask)*8)
u clipped to 8 bits afterwards, v to 9 bits
```

Mask/Offset are the raw 5-bit fields. Mask field N masks bits `8..8+N-1`
counted from the *third* bit up (fields are in 8-texel steps), producing a
repeat period of `(2^popcount_span)` texels; e.g. mask_x=1 repeats every 16
texels, mask_x=7 every 64.

## Transparency rule (fetch-side)

Texel colour `0000h` is fully transparent for every textured command.
Colour `8000h` (STP-flagged black) is opaque on opaque commands and handled
by the blend stage on semi-transparent commands (exercise 02).

## Worked example

Page at field=1 (base X=64), row 0 halfword 0 = `0x7654`.
Even-page semantics: texel u=0 is nibble 0 = `4`. With the odd-page flip:
texel u=0 is nibble 3 = `7`. With CLUT base (0,100), entry values
`0x111*(i+1)`: even page texel 0 renders `0x555`, odd page renders `0x888`.
