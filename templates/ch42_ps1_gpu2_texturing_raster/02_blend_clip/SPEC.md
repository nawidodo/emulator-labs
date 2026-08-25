# SPEC — 02_blend_clip

Scope: the pixel-output half of the rasterizer — blending modes, transparency
processing, dithering, exact drawing-area clipping (with the drawing offset
applied first) and GP0(E6h) mask bits. Fixture textures are 15bpp direct
colour so fetch never confounds the result.

## Pipeline stages (visible separation)

```
primitive_setup   shared/gpu_device.hpp -> BlendClipStages::screen_coord
rasterize         shared/gpu_device.hpp (blits + edge-function walk)
texture_fetch     blend_stages.hpp      (15bpp direct; not graded here)
blend_pixel       blend_stages.hpp      <- this exercise (marked blocks)
```

## Modulate macro

Per component, at 8-bit precision:

```
texel8  = expand5to8(texel5)          // (c<<3)|(c>>2), 31 -> 255
out8    = min(255, (texel8 * shade8) >> 7)
```

Shade `80h` is unity (`(255*128)>>7 = 255`); shades above `80h` overdrive and
saturate. The result is truncated to 5 bits only at the very end.

## Decal (raw texture)

Command bit24 selects raw-texture output: the texel lands in VRAM unmodified;
the primitive colour word is ignored.

## Semi-transparency

Enabled by command bit25; equation selected by Texpage bits 5-6, per 5-bit
component with saturation:

| mode | equation   |
|------|------------|
| 0    | B/2 + F/2  |
| 1    | B + F      |
| 2    | B - F      |
| 3    | B + F/4    |

## Transparency processing rules

- Texel with all colour bits zero (`x000h`, RGB=0) is **skipped**.
- Unless the STP flag (bit15) is set: `8000h` is drawable black — opaque on
  opaque commands, blended as black on semi-transparent commands.
- Written bit15 policy (read-modify-write epilogue in shared/gpu_device.hpp):
  forced to 1 when Set-mask (E6h bit0) is on, otherwise the texture's own
  STP flag is preserved into the framebuffer.

## Dithering

PSX-SPX table (offsets added to the 8-bit components, saturated, then >>3):

```
-4  +0  -3  +1
+2  -2  +3  -1
-3  +1  -4  +0
+3  -1  +2  -2      index = kDither[y & 3][x & 3]
```

Applied ONLY when a polygon uses modulation with the E1h dither flag set
(rectangles are never dithered; decal polys are not dithered).

## Worked example

Full-green texel (`0x03E0`), shade `80h`, no dither, semi mode 0 over black:

1. modulate: `(255*128)>>7 = 255` -> truncate -> F = 31
2. semi mode 0: `(B+F)/2 = (0+31)/2 = 15`
3. final pixel: `pack_bgr15(0,15,0) = 0x01E0`

The unit tests pin this arithmetic end to end.
