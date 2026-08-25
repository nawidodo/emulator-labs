# CODING_TEST — unseen textured primitive, exact VRAM hash

## The task

You are given a GP0 command stream you have never seen. It renders a
textured scene — an **8bpp CLUT texture on an odd texture page**, wrapped
through a GP0(E2h) texture window, drawn as two flat-shaded textured
triangles (`GP0(24h)`) forming a quadrilateral plus one variable-size
textured rectangle (`GP0(64h)`), all modulated (no decal, no semi-
transparency, dither disabled).

Implement the three marked stages in `coding_stages.hpp` so the runner
reproduces the reference framebuffer bit-exactly:

1. `in_draw_area` — exact inclusive drawing-area clip (`X1<=x<=X2`,
   `Y1<=y<=Y2`), applied AFTER the drawing offset.
2. `texture_fetch` — GP0(E2h) window wrap
   `(t & ~(Mask*8)) | ((Offset & Mask)*8)`, then per-depth fetch:
   4bpp = 4 nibble lanes per halfword / 64-halfword rows, 8bpp = 2 byte
   lanes / 128-halfword rows, 15bpp = direct; an ODD raw page-X field
   mirrors the lane order; CLUT lookup at `(clut.x + index, clut.y)`.
3. `blend_pixel` — expand texel components 5→8 via `(c<<3)|(c>>2)`,
   modulate `min(255,(tex*shade)>>7)` (decal copies the texel), optional
   `kDither[y&3][x&3]` offsets with saturation BEFORE the `>>3` truncation,
   and the four semi equations on 5-bit components when bit25 is set.

## Grading

```bash
./ch42_99_coding_test_runner \
    --rom tests/hidden/ch42_ps1_gpu2_texturing_raster/roms/unseen.bin \
    --hash-frame /tmp/out.hash
```

The hidden manifest pins the FNV-1a-64 of `/tmp/out.hash`'s payload (the
full VRAM dump). Every byte of VRAM must match the reference pipeline.

## Hints

- The stream is little-endian u32 words; the runner feeds them to
  `GpuDevice<CodingTestStages>` in order.
- Vertex words: X bits 0-10 signed, Y bits 16-26.
- Textured triangle: word0 colour+cmd, then (vertex, texcoord+CLUT),
  (vertex, texcoord+TEXPAGE), (vertex, texcoord).
- Quads are two triangles: vertices 1-2-3 and 2-3-4.
