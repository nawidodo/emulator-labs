#pragma once
// Exercise 01 — texture pages and texel fetch.
//
// A texture page is a 256x256-texel region whose base sits on X multiples of
// 64 halfwords and Y multiples of 256 lines. Depending on colour depth the
// page data is packed differently (PSX-SPX "Texture Bitmaps"):
//
//   4bpp : FOUR texels per halfword, lowest nibble = leftmost texel,
//          one page row spans 64 halfwords.
//   8bpp : TWO texels per halfword, low byte = leftmost texel,
//          one page row spans 128 halfwords.
//   15bpp: ONE texel per halfword, direct colour, row = 256 halfwords.
//
// The texel index selects a CLUT entry located at the primitive's CLUT base
// (a 16x1 or 256x1 halfword row in VRAM).
#include "../shared/gpu_device.hpp"
#include "../shared/gpu_state.hpp"

namespace psx::gpu {

// Stage: texture-window wrapping (GP0(E2h)).
// PSX-SPX: Texcoord = (Texcoord AND NOT(Mask*8)) OR ((Offset AND Mask)*8),
// where Mask/Offset are the raw 5-bit fields counted in 8-texel steps.
// The window defines a repeat period smaller than the full page; the GPU
// reads the repeated pattern as if it were stored across the whole page.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int wrap_u(int u, const TexWindow& w) {
    const int m = w.mask_x * 8;
    return ((u & ~m) | ((w.off_x & w.mask_x) * 8)) & 0xFF;
}

inline int wrap_v(int v, const TexWindow& w) {
    const int m = w.mask_y * 8;
    return ((v & ~m) | ((w.off_y & w.mask_y) * 8)) & 0x1FF;
}
//@LABS-STUB
inline int wrap_u(int u, const TexWindow& w) {
    (void)w;
    // TODO(1): apply the GP0(E2h) texture-window formula and clip to 8 bits.
    return u & 0xFF;
}

inline int wrap_v(int v, const TexWindow& w) {
    (void)w;
    // TODO(1): apply the GP0(E2h) texture-window formula and clip to 9 bits.
    return v & 0x1FF;
}
//@LABS-END

// Stage: 4bpp texel fetch through the colour lookup table.
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline uint16_t fetch_texel_4bpp(const Vram& vram, const DrawMode& mode,
                                 const Clut& clut, int u, int v) {
    const int page_x = mode.page_x_field * 64;
    const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 2)];
    // Hardware quirk: an odd texture-page X base mirrors the lane order
    // inside each halfword, so CLUT entry selection flips with it.
    int lane = u & 3;
    if ((mode.page_x_field & 1) != 0) lane = 3 - lane;
    const int index = (word >> (lane * 4)) & 0xF;
    return vram.at(clut.x + index, clut.y);
}
//@LABS-STUB
inline uint16_t fetch_texel_4bpp(const Vram& vram, const DrawMode& mode,
                                 const Clut& clut, int u, int v) {
    (void)vram;
    (void)mode;
    (void)clut;
    (void)u;
    (void)v;
    // TODO(2): locate the page halfword, pick the nibble lane, look up CLUT.
    return 0;
}
//@LABS-END

// Stage: 8bpp texel fetch through the colour lookup table.
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline uint16_t fetch_texel_8bpp(const Vram& vram, const DrawMode& mode,
                                 const Clut& clut, int u, int v) {
    const int page_x = mode.page_x_field * 64;
    const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 1)];
    int lane = u & 1;
    if ((mode.page_x_field & 1) != 0) lane = 1 - lane;
    const int index =
        lane != 0 ? static_cast<int>(word >> 8) : static_cast<int>(word & 0xFF);
    return vram.at(clut.x + index, clut.y);
}
//@LABS-STUB
inline uint16_t fetch_texel_8bpp(const Vram& vram, const DrawMode& mode,
                                 const Clut& clut, int u, int v) {
    (void)vram;
    (void)mode;
    (void)clut;
    (void)u;
    (void)v;
    // TODO(3): locate the page halfword, pick the byte lane, look up CLUT.
    return 0;
}
//@LABS-END

// Stage: 15bpp direct-colour texel fetch (no CLUT involved).
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline uint16_t fetch_texel_15bpp(const Vram& vram, const DrawMode& mode,
                                  int u, int v) {
    const int page_x = mode.page_x_field * 64;
    return vram.px[page_x + v * kVramWidth + u];
}
//@LABS-STUB
inline uint16_t fetch_texel_15bpp(const Vram& vram, const DrawMode& mode,
                                  int u, int v) {
    (void)vram;
    (void)mode;
    (void)u;
    (void)v;
    // TODO(4): read the halfword at the page-relative texel address.
    return 0;
}
//@LABS-END

// Glue implementing the shared device interface for this exercise. The
// blending-related stages are deliberately pass-through: this exercise draws
// raw-texture rectangles, so the interesting work is all in the fetch above.
struct TexPagesStages {
    // Stage: drawing-offset addition happens BEFORE any clipping.
    static int screen_coord(int v, int offset) { return v + offset; }

    // Stage: drawing area clip — both ends inclusive (X1<=x<=X2, Y1<=y<=Y2).
    static bool in_draw_area(int x, int y, const DrawArea& a) {
        return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
    }

    // Stage: dispatch by GP0(E1h) colour depth after window wrapping.
    static uint16_t texture_fetch(const TexEnv& env, int uf8, int vf8) {
        const Vram& vram = *env.vram;
        const int u = wrap_u(uf8 >> 8, env.win);
        const int v = wrap_v(vf8 >> 8, env.win);
        switch (env.mode.depth) {
            case 0: return fetch_texel_4bpp(vram, env.mode, env.clut, u, v);
            case 1: return fetch_texel_8bpp(vram, env.mode, env.clut, u, v);
            default: return fetch_texel_15bpp(vram, env.mode, u, v);
        }
    }

    // Stage: colour 0000h is fully transparent regardless of command type;
    // 8000h (STP-flagged black) is NOT skipped here.
    static bool transparency_skip(uint16_t texel, const PrimCtx&) {
        return texel == 0x0000;
    }

    // Stage: GP0(E6h) mask test — write-protected pixels have bit15 set.
    static bool mask_test(uint16_t bg, const PrimCtx& ctx) {
        return ctx.mask.test_bit && (bg & 0x8000) != 0;
    }

    // Stage: raw-texture passthrough (exercise 01 does not modulate).
    static uint16_t blend_pixel(uint16_t /*bg*/, uint16_t texel,
                                const PrimCtx&, int, int) {
        return texel;
    }
};

}  // namespace psx::gpu
