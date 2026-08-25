#pragma once
#include <array>
#include <cstdint>

// Chapter 21 — background fetch pipeline, simplified to a per-tile fetch
// model: for every 8x8 tile the PPU conceptually fetches
//   1. nametable byte   -> tile index
//   2. attribute byte   -> 2-bit palette select
//   3. pattern low/high planes from CHR
// and then shifts pixels out left-to-right, selecting with fine X.
// The real hardware overlaps these fetches with pixel output through
// shift registers; this chapter keeps the per-tile model because it is
// observably identical for static scenes (scrolling, which makes the
// difference visible mid-scanline, is Chapter 22 material).
//
namespace nes21bg {

// One common approximation of the 2C02 NTSC master palette. The exact DAC
// tuning varies between TVs/PPUs; what matters here is that the table is a
// fixed deterministic mapping so golden hashes are stable everywhere.
struct Rgb {
    uint8_t r, g, b;
};

inline const std::array<Rgb, 64>& master_palette() {
    static const std::array<Rgb, 64> kPalette{{
        // $0x row (darkest)
        {84, 84, 84}, {0, 30, 116}, {8, 16, 144}, {48, 0, 136},
        {68, 0, 100}, {92, 0, 48}, {84, 4, 0}, {60, 24, 0},
        {32, 42, 0}, {8, 58, 0}, {0, 64, 0}, {0, 60, 0},
        {0, 50, 60}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
        // $1x row
        {152, 150, 152}, {8, 76, 196}, {48, 50, 236}, {92, 30, 228},
        {136, 20, 176}, {160, 20, 100}, {152, 34, 32}, {120, 60, 0},
        {84, 90, 0}, {40, 114, 0}, {8, 124, 0}, {0, 118, 40},
        {0, 102, 120}, {0, 0, 0}, {0, 0, 0}, {236, 238, 236},
        // $2x row
        {236, 238, 236}, {76, 154, 236}, {120, 124, 236}, {176, 98, 236},
        {228, 84, 236}, {236, 88, 180}, {236, 106, 100}, {212, 136, 32},
        {160, 170, 0}, {116, 196, 0}, {76, 208, 32}, {56, 204, 108},
        {56, 180, 204}, {60, 60, 60}, {0, 0, 0}, {236, 238, 236},
        // $3x row (lightest)
        {236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236},
        {236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144},
        {204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180},
        {160, 214, 228}, {160, 162, 160}, {0, 0, 0}, {236, 238, 236},
    }};
    return kPalette;
}

// Each attribute byte covers a 4x4-tile region and packs four 2-bit palette
// selects, one per 2x2-tile quadrant:
//
//   bit layout        quadrant grid (coarse coords)
//   33 22 11 00       (x,y in {0,1} mod 2)
//   \_low two tiles
//
//   shift = ((coarse_y & 2) << 1) | (coarse_x & 2)
//   palette = (at_byte >> shift) & 3
//
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int attribute_bits(uint8_t at_byte, int coarse_x, int coarse_y) {
    int shift = ((coarse_y & 2) << 1) | (coarse_x & 2);
    return (at_byte >> shift) & 3;
}
//@LABS-STUB
// TODO(1): extract the 2-bit palette select for one coarse tile position.
// shift = ((coarse_y & 2) << 1) | (coarse_x & 2); result = (at>>shift)&3.
// The stub always reports quadrant 0 so color tests run RED.
inline int attribute_bits(uint8_t /*at_byte*/, int /*coarse_x*/, int /*coarse_y*/) {
    return 0;  // wrong on purpose
}
//@LABS-END

// Combine the two pattern planes into the 2-bit tile color for one pixel.
// Planes are bitplanes: bit (7 - x_in_tile) of `low` is plane 0, same bit
// of `high` is plane 1. Color 0 means "transparent" for backgrounds.
//
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline int tile_pixel(uint8_t low, uint8_t high, int x_in_tile) {
    int bit = 7 - (x_in_tile & 7);
    return ((low >> bit) & 1) | (((high >> bit) & 1) << 1);
}
//@LABS-STUB
// TODO(2): return the 2-bit color at x_in_tile by combining bit
// (7 - x_in_tile) of both planar bytes. Stub returns transparent.
inline int tile_pixel(uint8_t /*low*/, uint8_t /*high*/, int /*x_in_tile*/) {
    return 0;  // wrong on purpose
}
//@LABS-END

// Everything needed to render one static frame (no scroll, no sprites —
// those arrive in Chapter 22). The nametable pointer addresses ONE logical
// 2 KB nametable image already selected from physical VRAM.
struct FrameInputs {
    const uint8_t* chr;   // 8 KB pattern tables
    const uint8_t* nt;    // 2 KB logical nametable ($2000 window contents)
    const uint8_t* pal;   // 32-byte palette RAM
    uint8_t ctrl;         // only PPUCTRL bit 4 (bg pattern table) matters here
};

constexpr int kFrameW = 256;
constexpr int kFrameH = 240;

// Render one background pixel to a palette-RAM index. Hardware rule: tile
// color 0 never reads the bg palettes — every palette entry whose low bits
// are 0 shows the universal backdrop at $3F00 instead.
//
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline int render_pixel(const FrameInputs& in, int px, int py) {
    int coarse_x = px >> 3;
    int fine_y = py & 7;
    int tile_row = py >> 3;

    uint8_t tile = in.nt[tile_row * 32 + coarse_x];
    uint8_t at = in.nt[0x03C0 + (tile_row >> 2) * 8 + (coarse_x >> 2)];
    int attr = attribute_bits(at, coarse_x, tile_row);

    int plane_base = (in.ctrl & 0x10) ? 0x1000 : 0x0000;
    int offset = plane_base + tile * 16 + fine_y;
    uint8_t low = in.chr[offset];
    uint8_t high = in.chr[offset + 8];

    int color = tile_pixel(low, high, px & 7);
    if (color == 0) return 0x00;                 // universal backdrop
    return (attr << 2) | color;                  // index into $3F00-$3F1F
}
//@LABS-STUB
// TODO(3): compute the palette-RAM index for one pixel:
//   tile      = nt[(py>>3)*32 + (px>>3)]
//   attr      = nt[0x3C0 + (py>>5)*8 + (px>>5)] via attribute_bits()
//   pattern   = chr[(ctrl&0x10 ? 0x1000 : 0) + tile*16 + (py&7)] (+8 high)
//   color     = tile_pixel(low, high, px&7); color==0 -> backdrop $3F00.
// Return (attr<<2)|color, or 0 for the backdrop. Stub returns 0 always.
inline int render_pixel(const FrameInputs& /*in*/, int /*px*/, int /*py*/) {
    return 0;  // wrong on purpose
}
//@LABS-END

// Render a full 256x240 RGBA8 frame into `out` (245760 bytes).
template <size_t N>
void render_frame(std::array<uint8_t, N>& out, const FrameInputs& in) {
    static_assert(N == size_t(kFrameW) * kFrameH * 4,
                  "frame buffer must be 256*240*4 bytes");
    const auto& mpal = master_palette();
    for (int y = 0; y < kFrameH; ++y) {
        for (int x = 0; x < kFrameW; ++x) {
            int ram_idx = render_pixel(in, x, y) & 0x1F;
            const Rgb& c = mpal[in.pal[ram_idx]];
            size_t o = (size_t(y) * kFrameW + x) * 4;
            out[o + 0] = c.r;
            out[o + 1] = c.g;
            out[o + 2] = c.b;
            out[o + 3] = 0xFF;
        }
    }
}

}  // namespace nes21bg
