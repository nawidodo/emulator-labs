#pragma once
// Exercise 90 — seeded-bug debugging target.
//
// This is the complete ch42 pipeline (fetch + blend + clip) with TWO seeded
// bugs on the STUB side:
//
//   Bug A (texture_fetch): the odd-texture-page-X lane mirror is missing.
//     Real hardware mirrors nibble/byte lanes inside each halfword when the
//     raw page_x_field of GP0(E1h) is odd; the buggy fetch always reads
//     lane u&3 (4bpp) / low-high order (8bpp).
//   Bug B (in_draw_area): the drawing area's bottom/right edges are
//     excluded (`<` instead of `<=`), so column X2 and row Y2 are never
//     drawn even though the area bounds are inclusive.
//
// Every other stage is verbatim-correct so failing tests isolate these two.
#include "../shared/gpu_device.hpp"
#include "../shared/gpu_state.hpp"

namespace psx::gpu {

namespace detail {

inline uint16_t debug_fetch(const Vram& vram, const DrawMode& mode,
                            const Clut& clut, int u, int v) {
    const int page_x = mode.page_x_field * 64;
    switch (mode.depth) {
        case 0: {
            const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 2)];
            int lane = u & 3;
//@LABS-BEGIN 1
//@LABS-SOLUTION
            // Odd page X mirrors the lane order inside the halfword.
            if ((mode.page_x_field & 1) != 0) lane = 3 - lane;
//@LABS-STUB
            // BUG A: no lane mirroring on odd texture-page X bases.
            // TODO(1)
//@LABS-END
            const int index = (word >> (lane * 4)) & 0xF;
            return vram.at(clut.x + index, clut.y);
        }
        case 1: {
            const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 1)];
            int lane = u & 1;
//@LABS-BEGIN 3
//@LABS-SOLUTION
            if ((mode.page_x_field & 1) != 0) lane = 1 - lane;
            const int index = lane != 0 ? static_cast<int>(word >> 8)
                                        : static_cast<int>(word & 0xFF);
//@LABS-STUB
            // BUG A: byte order never flips either.
            const int index = lane != 0 ? static_cast<int>(word >> 8)
                                        : static_cast<int>(word & 0xFF);
            // TODO(3)
//@LABS-END
            return vram.at(clut.x + index, clut.y);
        }
        default:
            return vram.px[page_x + v * kVramWidth + u];
    }
}

}  // namespace detail

struct DebugStages {
    static int screen_coord(int v, int offset) { return v + offset; }

    static bool in_draw_area(int x, int y, const DrawArea& a) {
//@LABS-BEGIN 2
//@LABS-SOLUTION
        // The area corners are inclusive at BOTH ends.
        return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
//@LABS-STUB
        // BUG B: bottom/right edge excluded — drops column X2 and row Y2.
        return x >= a.x1 && x < a.x2 && y >= a.y1 && y < a.y2;
        // TODO(2)
//@LABS-END
    }

    static uint16_t texture_fetch(const TexEnv& env, int uf8, int vf8) {
        const Vram& vram = *env.vram;
        const TexWindow& w = env.win;
        const int u = (((uf8 >> 8) & ~(w.mask_x * 8)) |
                       ((w.off_x & w.mask_x) * 8)) & 0xFF;
        const int v = (((vf8 >> 8) & ~(w.mask_y * 8)) |
                       ((w.off_y & w.mask_y) * 8)) & 0x1FF;
        return detail::debug_fetch(vram, env.mode, env.clut, u, v);
    }

    static bool transparency_skip(uint16_t texel, const PrimCtx&) {
        return (texel & 0x7FFF) == 0 && (texel & 0x8000) == 0;
    }

    static bool mask_test(uint16_t bg, const PrimCtx& ctx) {
        return ctx.mask.test_bit && (bg & 0x8000) != 0;
    }

    static uint16_t blend_pixel(uint16_t bg, uint16_t texel,
                                const PrimCtx& ctx, int x, int y) {
        uint16_t fg;
        if (!ctx.textured) {
            fg = ctx.shade15;
        } else if (ctx.raw) {
            fg = texel;
        } else {
            const int t[3] = {expand5to8(static_cast<uint8_t>(texel & 0x1F)),
                              expand5to8(static_cast<uint8_t>((texel >> 5) & 0x1F)),
                              expand5to8(static_cast<uint8_t>((texel >> 10) & 0x1F))};
            const int s[3] = {ctx.shade_r, ctx.shade_g, ctx.shade_b};
            int rgb[3];
            for (int i = 0; i < 3; ++i)
                rgb[i] = (t[i] * s[i]) >> 7 > 255 ? 255 : (t[i] * s[i]) >> 7;
            if (ctx.dither) {
                for (int i = 0; i < 3; ++i) {
                    int val = rgb[i] + kDither[y & 3][x & 3];
                    rgb[i] = val < 0 ? 0 : (val > 255 ? 255 : val);
                }
            }
            fg = pack_bgr15(rgb[0] >> 3, rgb[1] >> 3, rgb[2] >> 3);
        }
        if (!ctx.semi) return fg;
        const Rgb5 b = unpack_bgr15(bg);
        const Rgb5 f = unpack_bgr15(fg);
        auto eq = [m = ctx.semi_mode](int bv, int fv) -> int {
            switch (m) {
                case 0: return (bv + fv) / 2;
                case 1: return bv + fv;
                case 2: return bv - fv;
                default: return bv + fv / 4;
            }
        };
        return pack_bgr15(eq(b.r, f.r), eq(b.g, f.g), eq(b.b, f.b));
    }
};

}  // namespace psx::gpu
