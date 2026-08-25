# 05_compositor — SPEC

Unify all layers into one scanline compositor.

1. `window_mask` — WIN0/WIN1 rectangles (half-open, inverted = disabled),
   WININ/WINOUT masks: bits 0-3 BG0-BG3, 4 OBJ, 5 blend-enable.
2. `mosaic_quantize` — snap sample coordinates to block origins; register
   values are size-1.
3. `bg_layer_pixel` — mode dispatch (3/5 direct, 4 paletted with transparent
   index 0, 1/2 affine via latched counters, 0/1 text), DISPCNT enables,
   BG mosaic on character layers.
4. `obj_layer_pixel` — best sprite by priority then lowest OAM index,
   wrap-around positioning, affine matrices, hidden non-affine double-size.
5. `pick_layers` — compositing order: priority value asc, OBJ wins ties vs
   BG, lower BG id wins ties vs BG.
6. `apply_bld` — alpha blend (EVA/EVB clamped to 16), lighten/darken via
   BLDY, semi-transparent sprites force the alpha path.

The provided `compose_frame` glues everything into a 240x160 RGBA8888 frame;
the runner (`ch28_ppu_runner`) renders `.pps` scripts headless and emits
FNV-64 frame hashes and per-scanline traces.

Known documented simplifications: no OBJ-window masking, doubled-size sprite
boxes anchor at the sprite origin, sprite mosaic omitted.
