# 03_affine_transforms — SPEC

Implement GBA affine background math and rendering.

1. `fixed_mul` (8.8 x 8.8 -> 8.8 on s32) and `AffineParams::decode` of the
   PA/PB/PC/PD + DX/DY register blocks (BG2 at IO+0x20, BG3 at IO+0x30).
2. `latch_line` — per-scanline internal counters: reference point plus
   PB/PD scaled by the line number.
3. `texel_coord` — per-column step by PA/PC.
4. `affine_texel` — wrap coordinates modulo the power-of-two texture edge,
   byte screen entries, 64-byte 8bpp tiles, color 0 transparent.
5. `render_affine_scanline` — full 240-pixel line into palette indices.

Acceptance: identity transform maps texels one-to-one; a scroll past the
texture edge wraps seamlessly; pa=pd=2.0 zooms 2x.
