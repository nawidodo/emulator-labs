# 02_text_backgrounds — SPEC

Implement GBA text background rendering (modes 0/1).

1. `decode_text_bg_config` — full BGnCNT decode incl. size table
   (256x256 / 512x256 / 256x512 / 512x512 pixels).
2. `decode_screen_entry` — tile, hflip/vflip, palette bank.
3. `tile_pixel` — 4bpp (nibble rows, bank*16+n) and 8bpp fetch; index 0 is
   transparent (-1).
4. `text_bg_pixel` — scroll (9-bit wrap), map wrap by pixel size, quadrant
   block addressing for maps > 256x256, flip handling.
5. `compose_text_scanline` — lowest priority value wins; ties keep the lower
   BG number; -1 = backdrop.

Acceptance: all tests green; scroll of 520 behaves as scroll of 8.
