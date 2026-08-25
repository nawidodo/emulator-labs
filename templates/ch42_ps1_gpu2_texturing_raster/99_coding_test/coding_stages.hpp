#pragma once
// 99_coding_test — annotated starter for the unseen-spec coding test.
// The hidden grading fixture renders a DIFFERENT textured primitive stream
// through this same pipeline and compares the exact VRAM hash.
#include "../shared/gpu_device.hpp"
#include "../shared/gpu_state.hpp"

namespace psx::gpu {

struct CodingTestStages {
    static int screen_coord(int v, int offset) { return v + offset; }

    static bool transparency_skip(uint16_t texel, const PrimCtx&) {
        return (texel & 0x7FFF) == 0 && (texel & 0x8000) == 0;
    }

    static bool mask_test(uint16_t bg, const PrimCtx& ctx) {
        return ctx.mask.test_bit && (bg & 0x8000) != 0;
    }

    // Stage: drawing area clip — inclusive at both ends.
    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    static bool in_draw_area(int x, int y, const DrawArea& a) {
        return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
    }
    //@LABS-STUB
    static bool in_draw_area(int, int, const DrawArea&) {
        // TODO(1): implement X1<=x<=X2 && Y1<=y<=Y2.
        return true;
    }
    //@LABS-END

    // Stage: texel fetch — window wrap plus 4bpp/8bpp CLUT lookup with the
    // odd-page lane mirror, or direct 15bpp.
    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    static uint16_t texture_fetch(const TexEnv& env, int uf8, int vf8) {
        const Vram& vram = *env.vram;
        const TexWindow& w = env.win;
        const int u = (((uf8 >> 8) & ~(w.mask_x * 8)) |
                       ((w.off_x & w.mask_x) * 8)) & 0xFF;
        const int v = (((vf8 >> 8) & ~(w.mask_y * 8)) |
                       ((w.off_y & w.mask_y) * 8)) & 0x1FF;
        const DrawMode& m = env.mode;
        const int page_x = m.page_x_field * 64;
        switch (m.depth) {
            case 0: {
                const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 2)];
                int lane = u & 3;
                if ((m.page_x_field & 1) != 0) lane = 3 - lane;
                return vram.at(env.clut.x + ((word >> (lane * 4)) & 0xF),
                               env.clut.y);
            }
            case 1: {
                const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 1)];
                int lane = u & 1;
                if ((m.page_x_field & 1) != 0) lane = 1 - lane;
                const int idx = lane != 0 ? static_cast<int>(word >> 8)
                                          : static_cast<int>(word & 0xFF);
                return vram.at(env.clut.x + idx, env.clut.y);
            }
            default:
                return vram.px[page_x + v * kVramWidth + u];
        }
    }
    //@LABS-STUB
    static uint16_t texture_fetch(const TexEnv&, int, int) {
        // TODO(2): window-wrap the texel coords and fetch by colour depth.
        return 0;
    }
    //@LABS-END

    // Stage: blend — modulate/decal, optional dither, optional semi mode.
    //@LABS-BEGIN 3
    //@LABS-SOLUTION
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
    //@LABS-STUB
    static uint16_t blend_pixel(uint16_t /*bg*/, uint16_t /*texel*/,
                                const PrimCtx& /*ctx*/, int /*x*/, int /*y*/) {
        // TODO(3): modulate (or decal), dither, then semi-transparent blend.
        return 0;
    }
    //@LABS-END
};

}  // namespace psx::gpu
