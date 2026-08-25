# 04_sprites — SPEC

Implement GBA sprite (OAM) rendering primitives.

1. `decode_obj_attrs` — ATTR0/1/2 of an OAM slot incl. the shape/size table
   and 256-color flag; flips ignored while affine.
2. `sprite_pixel_byte` — byte offset honoring 1D vs 2D mapping
   (DISPCNT bit 5) and 4bpp/8bpp tile sizes.
3. `obj_pixel_index` — flips, transparency (index 0), final palette index
   `256 + bank*16 + n`.
4. `load_obj_matrix` / `affine_obj_texel` — OAM-interleaved s16 matrices,
   centered inverse-mapping in 8.8 fixed point, double-size ranges.
5. `resolve_sprite_vs_bg` — lower priority value wins; equal priority goes
   to the sprite; transparent sprites never occlude.

Acceptance: all tests green, including a 90-degree rotation mapping.
