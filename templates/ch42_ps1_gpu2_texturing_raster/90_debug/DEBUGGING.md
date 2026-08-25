# DEBUGGING — 90_debug

Two independent bugs are seeded into the reference pipeline. The skeleton
builds and runs; the regression tests below FAIL on the buggy stubs and PASS
once you fix them. Write `bug-report.md` (bug / root cause / first
divergence / fix / regression test) for each.

## Bug A — CLUT lane order on odd texture pages

**Where:** `detail::debug_fetch`, 4bpp and 8bpp cases.

**Symptom:** textures sampled from a texture page whose raw GP0(E1h) X field
is odd render with horizontally scrambled colours. The damage is periodic:
4bpp images swap colour in groups of four texels, 8bpp in pairs — every
other halfword's lanes are read in the wrong order, so the CLUT entry
selection is mirrored. Even-page textures are pixel-perfect, which makes the
glitch look like "corrupt palette data" at first glance.

**First divergence:** the very first texel fetched from an odd page: u=0
reads nibble/byte lane 0 instead of the mirrored high lane.

**Root cause:** real hardware mirrors the lane order inside each texture
halfword when `page_x_field` (GP0(E1h) bits 0-3) is odd. The buggy code
always reads lane `u&3` / the low byte first.

**Fix:** invert the intra-halfword lane selection when
`(mode.page_x_field & 1) != 0`: 4bpp `lane = 3 - (u & 3)`, 8bpp
`lane = 1 - (u & 1)`.

**Regression test:** `debug.regression_odd_page_lane_flip`,
`debug.regression_odd_page_byte_flip_8bpp`.

## Bug B — drawing area off-by-one

**Where:** `DebugStages::in_draw_area`.

**Symptom:** the last column (X2) and last row (Y2) of the drawing area are
never rendered. Primitives that straddle the area boundary lose exactly one
pixel row/column along their right/bottom clip edge; full-screen clears via
rectangles show a 1-pixel dark seam at the frame edges. Golden VRAM hashes
mismatch only in primitives touching the boundary.

**First divergence:** the single pixel drawn at x == area.x2 (or y ==
area.y2): hardware draws it, the buggy build skips it.

**Root cause:** PSX-SPX defines the drawing area corners as INCLUSIVE at
both ends (`X1 <= x <= X2 && Y1 <= y <= Y2`). The seeded code compares with
strict `<` against X2/Y2.

**Fix:** use `<=` for both upper bounds.

**Regression test:** `debug.regression_clip_corner_inclusive`.

## Reproducing end to end

```bash
./ch42_90_debug_runner --rom tests/public/ch42_ps1_gpu2_texturing_raster/fixtures/debug_probe.bin \
    --hash-frame /tmp/probe.hash
```

The fixture draws an odd-page 4bpp rect clipped by a tight drawing area, so
both bugs corrupt the same frame; compare `/tmp/probe.hash` with the golden
before/after your fix.
