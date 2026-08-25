#pragma once
// 91_challenge — complete reference pipeline (no @LABS blocks): this
// exercise is about assembling a correct GP0 command stream that drives the
// renderer to a golden VRAM hash, not about implementing the stages.
#include "../shared/gpu_device.hpp"
#include "../shared/gpu_state.hpp"

namespace psx::gpu {

struct ChallengeStages {
    static int screen_coord(int v, int offset) { return v + offset; }

    static bool in_draw_area(int x, int y, const DrawArea& a) {
        return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
    }

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
                const int index = (word >> (lane * 4)) & 0xF;
                return vram.at(env.clut.x + index, env.clut.y);
            }
            case 1: {
                const uint16_t word = vram.px[page_x + v * kVramWidth + (u >> 1)];
                int lane = u & 1;
                if ((m.page_x_field & 1) != 0) lane = 1 - lane;
                const int index = lane != 0 ? static_cast<int>(word >> 8)
                                            : static_cast<int>(word & 0xFF);
                return vram.at(env.clut.x + index, env.clut.y);
            }
            default:
                return vram.px[page_x + v * kVramWidth + u];
        }
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
