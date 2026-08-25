# 01_bitmap_modes — SPEC

Implement the GBA bitmap display modes over the flat `PpuMemory` model.

1. `bgr555_to_rgba8888` — BGR555 to opaque RGBA8888 with bit replication
   `(v5 << 3) | (v5 >> 2)` (31 -> 255 exactly).
2. Mode 3 pixel store/load: one u16 per pixel, stride 240, VRAM base
   `0x06000000`. Out-of-range writes are ignored.
3. Mode 4: page base `0xA000` when DISPCNT bit 4 is set; 8-bit indices packed
   into u16s — setting one pixel must not disturb its horizontal neighbor.
   Displayed palette comes from BG palette RAM entries 0-255.
4. Geometry table for modes 3/4/5 plus the forced-blank flag (DISPCNT bit 6).
5. `render_bitmap_frame`: full 240x160 RGBA8888 frame. Forced blank = white;
   mode 5 shows backdrop PAL[0] outside its 160x128 frame.

Acceptance: all tests in `main.cpp` pass; rendering is byte-deterministic
(same memory state -> same FNV-64 frame digest).
