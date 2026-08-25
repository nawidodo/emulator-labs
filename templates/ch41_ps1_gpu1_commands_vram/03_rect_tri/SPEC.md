# SPEC — Exercise 03: untextured primitive rasterizer

This is the chapter STARTER exercise: the software rasterizer that the
challenge and coding test build upon. Implement the seven TODO blocks in
`raster.hpp`. Every rule below is normative — hidden goldens hash VRAM
images produced by exactly these semantics.

## Scope

- GP0(20h..23h) monochrome triangle (semi-transparent variants draw opaque
  this chapter; blending is ch42)
- GP0(28h..2Bh) monochrome quad, processed as triangles (1,2,3)+(2,3,4)
  per PSX-SPX
- GP0(30h..33h) shaded (Gouraud) triangle; GP0(38h..3Bh) shaded quad
- GP0(60h..62h, 65h-class variable rects) variable-size monochrome rectangle

Dithering is DISABLED for this whole chapter (the dither flag only affects
gouraud/textured output anyway and its matrix is a ch42 topic).

## Normative rasterization rules

1. **Sample rule.** Pixel (px,py) is owned by its centre
   `(px+0.5, py+0.5)` — equivalently the integer lattice point
   `(2*px+1, 2*py+1)`.
2. **Signed area / culling.** `area2 = (b-a) x (c-a)` in y-down screen
   space. `area2 <= 0` draws NOTHING; positive (clockwise on screen)
   faces the viewer.
3. **Edge functions.** For each edge `v_i -> v_j`:
   `E(P) = (x_j-x_i)*(P_y-y_i) - (y_j-y_i)*(P_x-x_i)` on the doubled
   lattice. Interior: every `E >= 0`. Incremental walk: one pixel right adds
   `-2*dy`, one row down adds `+2*dx` (lattice units).
4. **Top-left fill convention.** A boundary centre (`E == 0`) belongs to the
   primitive iff the edge is TOP (`dy == 0 && dx > 0`) or LEFT (`dy > 0`).
   Two adjacent primitives traverse their shared edge in the same
   direction, so they classify it identically — no double-drawn shared
   edges between separately submitted triangles.
5. **Quads.** Literal PSX-SPX split (v1,v2,v3)+(v2,v3,v4). Note: with rule 4
   the two halves may leave a thin sliver near the v1 corner undrawn; that
   is documented hardware-faithful behaviour for perimeter ordering.
6. **Gouraud color.** Weight of vertex k = normalized opposite-edge value:
   `lambda_k = (E_k << 12) / (4 * area2)` (truncating division; all terms
   non-negative inside). Channel =
   `(lambda_a*c_a + lambda_b*c_b + lambda_c*c_c + 2048) >> 12` (round
   half-up), clamped to 0..255, then truncated to 5 bits (`>> 3`) when
   packed to BGR555.
7. **Rectangles.** Size fields normalise like COPY commands:
   `((size-1) & mask)+1` — size 0 means maximum (1024 x 512). The drawing
   offset applies; pixels clip against the drawing area (inclusive corners).
8. **Pixel write.** check-mask set -> destinations with bit15=1 are
   write-protected. Otherwise store the BGR555 value, ORing bit15 when
   set-mask is on.

## Acceptance

`ch41_03_rect_tri_tests`: RED on skeleton, GREEN when done. It pins exact
pixel sets (top-left rule), culling, mask behaviour, quad sliver, and two
hand-computed Gouraud values.
