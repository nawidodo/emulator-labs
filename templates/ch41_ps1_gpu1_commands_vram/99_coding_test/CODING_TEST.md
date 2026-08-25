# CODING TEST — unseen triangle to an exact VRAM hash

## Task

The GPU in this directory is complete EXCEPT for the triangle rasterization
core, left as five LABS tasks in `gpu.hpp`:

| seq | function | what it decides |
|-----|----------|-----------------|
| 1 | `signed_area2` | front/back test (culling) |
| 2 | `is_top_left` | which edge boundaries are drawn |
| 3 | `draw_triangle_flat` | monochrome triangle walk |
| 4 | `shade_channel` | Q12 weighted color resolve |
| 5 | `draw_triangle_gouraud` | shaded triangle walk |

A hidden fixture streams an UNSEEN triangle state (drawing area, drawing
offset, mask attributes and one or two triangles: flat `20h`/quad `28h`,
Gouraud `30h`) through the provided pipeline. Your implementation must
reproduce the reference VRAM image bit-exactly:

```bash
ch41_99_coding_test_runner \
    --rom tests/hidden/ch41_ps1_gpu1_commands_vram/roms/tri_unseen.bin \
    --frames 1 --hash-frame out.txt
# out.txt must contain fnv64=<golden>
```

## Normative rules (identical to exercise 03 SPEC)

1. Pixel centre sample rule: pixel (px,py) is owned by (px+0.5, py+0.5),
   evaluated on the doubled lattice as (2*px+1, 2*py+1).
2. Signed area `(b-a) x (c-a)` in y-down screen space; area <= 0 draws
   nothing (backface cull).
3. Edge functions on the doubled lattice; interior requires every E >= 0;
   per-pixel steps: -2*dy horizontally, +2*dx per row.
4. Top-left fill convention: boundary centre drawn iff the owning edge has
   dy > 0 (left) or dy == 0 && dx > 0 (top).
5. Gouraud: lambda_k = (E_k << 12) / (4 * area2), truncating division;
   channel = (sum lambda_k * c_k + 2048) >> 12, clamped 0..255; packed
   15-bit by truncating each channel >> 3.
6. Quads split literally into triangles (v1,v2,v3)+(v2,v3,v4).
7. Clipping against GP0(E3h)/(E4h) drawing area corners inclusive;
   drawing offset (E5h) added before clipping; mask bits (E6h) honoured.
8. Dither disabled.

The hidden fixture uses only positive-area triangles whose vertices fit in
signed 11-bit fields, so no undefined behaviour is reachable if you follow
the rules above.

## Grading

`tests/hidden/ch41_ps1_gpu1_commands_vram/manifest.json` case
`coding_test_unseen_triangle` hashes your runner's full-VRAM dump with
FNV-1a-64 and compares against the golden produced by the reference
implementation. Partial credit does not exist: one wrong pixel fails.
