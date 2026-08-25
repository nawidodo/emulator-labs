#pragma once
// Exercise 04 — Mode 7 per-pixel affine transformation.
//
// Mode 7 warps one 1024x1024-pixel layer (a 128x128 byte tilemap whose bytes
// are also the tile numbers; each tile is 64 bytes = one byte per pixel,
// value = direct CGRAM index) onto the screen through an 8.8 fixed-point
// matrix.
//
// Per-screen-pixel computation (our exact model):
//
//   cx = sign_extend_13(x0)      cy = sign_extend_13(y0)
//   sx = x - cx                  sy = y - cy
//   u = A*sx + B*sy + (hofs << 8)        (all products in 8.8)
//   v = C*sx + D*sy + (vofs << 8)
//   px = floor(u / 256)          py = floor(v / 256)     screen->map pixels
//   tx = px >> 3                 ty = py >> 3            map tile column/row
//
// Out-of-range handling — we keep two of the hardware's four range/slchar
// behaviors and DOCUMENT the simplification:
//   wrap == false : any coordinate outside [0,1024) renders the BACKDROP
//                   (CGRAM entry 0). This merges hardware modes 0/1
//                   ("value > $FF -> transparent character") with mode 2's
//                   repeat; per-character flip repetition is not modeled.
//   wrap == true  : coordinates wrap modulo 1024 (hardware mode 3).
// Color 0 inside a tile is transparent in both cases and also falls back to
// the backdrop.
//
// The matrix registers are signed 13-bit values in 8.8 format; A = $0100
// with B = C = D = 0 and zero offsets/center is the identity mapping.
// Shared screen/VRAM/color model comes from exercise 03 so that the
// challenge runner can drive both renderers from one memory image.
#include "../03_bg_render/render.hpp"
#include <cstddef>

namespace snesbus {

struct Mode7Params {
    int16_t a = 0x0100;
    int16_t b = 0;
    int16_t c = 0;
    int16_t d = 0x0100;
    uint16_t x0 = 0;    // 13-bit signed center X
    uint16_t y0 = 0;    // 13-bit signed center Y
    uint16_t hofs = 0;  // 8.8 horizontal offset ($210D/$210E pair, low form)
    uint16_t vofs = 0;
    bool wrap = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Raw 8.8 fixed-point source coordinates for one screen pixel.
struct Mode7Raw {
    int32_t u = 0;
    int32_t v = 0;
};

inline Mode7Raw mode7_transform(const Mode7Params& p, int x, int y) {
    // Centers are 13-bit signed; sign-extend before subtracting.
    const int cx = (static_cast<int16_t>(p.x0 << 3)) >> 3;
    const int cy = (static_cast<int16_t>(p.y0 << 3)) >> 3;
    const int sx = x - cx;
    const int sy = y - cy;
    Mode7Raw r;
    r.u = p.a * sx + p.b * sy + (static_cast<int>(p.hofs) << 8);
    r.v = p.c * sx + p.d * sy + (static_cast<int>(p.vofs) << 8);
    return r;
}
//@LABS-STUB
struct Mode7Raw {
    int32_t u = 0;
    int32_t v = 0;
};

// TODO(1): compute the raw 8.8 source coordinates: sign-extend the 13-bit
// centers, then u = A*sx + B*sy + (hofs<<8), v = C*sx + D*sy + (vofs<<8).
inline Mode7Raw mode7_transform(const Mode7Params&, int, int) {
    return Mode7Raw{};  // wrong on purpose: every pixel samples map origin
}
//@LABS-END

struct MapPos {
    int px = 0;
    int py = 0;
    int tx = 0;
    int ty = 0;
    bool in_range = true;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Convert raw 8.8 coordinates to map position. Arithmetic shifts give
// floor semantics for negatives, matching the PPU's fixed-point unit.
inline MapPos to_map_pos(const Mode7Raw& raw, bool wrap) {
    MapPos m;
    m.px = raw.u >> 8;
    m.py = raw.v >> 8;
    m.tx = m.px >> 3;
    m.ty = m.py >> 3;
    if (wrap) {
        m.tx &= 127;
        m.ty &= 127;
    } else {
        m.in_range = m.tx >= 0 && m.tx < 128 && m.ty >= 0 && m.ty < 128;
    }
    return m;
}
//@LABS-STUB
// TODO(2): divide by shifting (px = u>>8, py = v>>8; tx = px>>3, ty =
// py>>3); when !wrap mark out-of-[0,128) tiles, when wrap reduce mod 128.
inline MapPos to_map_pos(const Mode7Raw&, bool) {
    return MapPos{};  // wrong on purpose: always claims tile (0,0) valid
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Fetch one Mode 7 pixel: map byte at (tx,ty) IS the tile number; each tile
// is 64 bytes of direct CGRAM indices. Returns 0 for out-of-range or
// transparent (color 0) samples — callers substitute the backdrop.
inline uint8_t fetch_mode7_pixel(const Vram& vram, uint16_t map_base,
                                 const MapPos& pos) {
    if (!pos.in_range) {
        return 0;
    }
    const size_t map_addr = static_cast<size_t>(map_base) * 2u +
                            static_cast<size_t>(pos.ty) * 128u +
                            static_cast<size_t>(pos.tx);
    const uint8_t tile = vram_byte(vram, map_addr);
    const size_t px = static_cast<size_t>(pos.px) & 7u;
    const size_t py = static_cast<size_t>(pos.py) & 7u;
    return vram_byte(vram, static_cast<size_t>(tile) * 64u + py * 8u + px);
}
//@LABS-STUB
// TODO(3): read the map byte (map_base*2 + ty*128 + tx), treat it as the
// tile number and return the byte at tile*64 + py%8*8 + px%8. Zero means
// transparent/out-of-range.
inline uint8_t fetch_mode7_pixel(const Vram&, uint16_t, const MapPos&) {
    return 0;  // wrong on purpose: everything renders as backdrop
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Render a full 256x224 Mode 7 frame into `out` (RGBA8888).
inline void render_mode7_frame(const Mode7Params& params, const Vram& vram,
                               const Cgram& cgram, uint16_t map_base,
                               std::span<uint8_t> out) {
    for (int y = 0; y < kScreenHeight; ++y) {
        for (int x = 0; x < kScreenWidth; ++x) {
            const Mode7Raw raw = mode7_transform(params, x, y);
            const MapPos pos = to_map_pos(raw, params.wrap);
            uint8_t color = fetch_mode7_pixel(vram, map_base, pos);
            const uint32_t rgba = bgr555_to_rgba8(cgram.e[color]);
            const size_t o = (static_cast<size_t>(y) * kScreenWidth +
                              static_cast<size_t>(x)) * 4u;
            out[o] = static_cast<uint8_t>(rgba);
            out[o + 1] = static_cast<uint8_t>(rgba >> 8);
            out[o + 2] = static_cast<uint8_t>(rgba >> 16);
            out[o + 3] = static_cast<uint8_t>(rgba >> 24);
        }
    }
}
//@LABS-STUB
// TODO(4): loop every pixel, transform -> map position -> fetch, fall back
// to CGRAM entry 0 on transparency, expand and store RGBA.
inline void render_mode7_frame(const Mode7Params&, const Vram&, const Cgram&,
                               uint16_t, std::span<uint8_t> out) {
    // Wrong on purpose: flat magenta placeholder frame.
    for (size_t i = 0; i < out.size(); i += 4) {
        out[i] = 0xFF;
        out[i + 1] = 0x00;
        out[i + 2] = 0xFF;
        out[i + 3] = 0xFF;
    }
}
//@LABS-END

}  // namespace snesbus
