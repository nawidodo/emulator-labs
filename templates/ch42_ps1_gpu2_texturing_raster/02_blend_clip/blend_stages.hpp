#pragma once
// Exercise 02 — texture blending modes, transparency processing, dithering,
// drawing area clipping and mask bits.
//
// The fixture textures here are 15bpp direct colour so the fetch stage is
// trivial and every marked block isolates one blend/clip rule (PSX-SPX):
//
//   Modulate : out = texel8 * shade8 >> 7   per component, saturated to 255
//              (shade 80h = unity; FFh overdrives ~2x before saturation)
//   Decal    : out = texel                 (command bit24 "raw-texture")
//   Semi     : B/2+F/2 | B+F | B-F | B+F/4 on 5-bit components
//   Dither   : kDither[4][4] offsets added to 8-bit values BEFORE >>3;
//              polygons with texture blending only, NEVER rectangles
#include "../shared/gpu_device.hpp"
#include "../shared/gpu_state.hpp"

namespace psx::gpu {

// Stage: drawing offset. The GP0(E5h) offset is added to vertex coordinates
// BEFORE any clip test — shifting a primitive can move it into or out of the
// drawing area entirely.
//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int offset_coord(int v, int offset) { return v + offset; }
//@LABS-STUB
inline int offset_coord(int v, int /*offset*/) {
    // TODO(1): add the GP0(E5h) drawing offset to the vertex coordinate.
    return v;
}
//@LABS-END

// Stage: drawing area clip. EXACTLY inclusive at both ends:
// X1 <= x <= X2 and Y1 <= y <= Y2.
//@LABS-BEGIN 2
//@LABS-SOLUTION
inline bool clip_in_draw_area(int x, int y, const DrawArea& a) {
    return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
}
//@LABS-STUB
inline bool clip_in_draw_area(int x, int y, const DrawArea& a) {
    // TODO(2): implement the inclusive drawing-area bounds test.
    (void)x;
    (void)y;
    (void)a;
    return true;
}
//@LABS-END

// Stage: transparency processing. A texel whose RGB bits are all zero is
// fully transparent UNLESS its STP flag (bit15) is set; STP-flagged black
// stays drawable and participates in semi-transparency as black.
//@LABS-BEGIN 3
//@LABS-SOLUTION
inline bool texel_transparent(uint16_t texel) {
    return (texel & 0x7FFF) == 0 && (texel & 0x8000) == 0;
}
//@LABS-STUB
inline bool texel_transparent(uint16_t texel) {
    // TODO(3): skip only all-zero-RGB pixels without the STP flag.
    (void)texel;
    return false;
}
//@LABS-END

// Stage: GP0(E6h) mask test. When Check-mask is enabled, pixels whose
// destination bit15 is already set are write-protected.
//@LABS-BEGIN 4
//@LABS-SOLUTION
inline bool mask_blocks(uint16_t bg, const MaskSetting& m) {
    return m.test_bit && (bg & 0x8000) != 0;
}
//@LABS-STUB
inline bool mask_blocks(uint16_t bg, const MaskSetting& m) {
    // TODO(4): block writes onto destinations with bit15 set when enabled.
    (void)bg;
    (void)m;
    return false;
}
//@LABS-END

// Stage: modulation macro. Operates at 8-bit precision — texel components
// are expanded 5->8 bits by replication first. out = (tex * shade) >> 7,
// saturated to 255 ("80h is brightest"; brighter shades overdrive).
//@LABS-BEGIN 5
//@LABS-SOLUTION
inline void modulate_rgb(const PrimCtx& ctx, uint16_t texel, int out[3]) {
    const int t[3] = {expand5to8(static_cast<uint8_t>(texel & 0x1F)),
                      expand5to8(static_cast<uint8_t>((texel >> 5) & 0x1F)),
                      expand5to8(static_cast<uint8_t>((texel >> 10) & 0x1F))};
    const int s[3] = {ctx.shade_r, ctx.shade_g, ctx.shade_b};
    for (int i = 0; i < 3; ++i) {
        const int v = (t[i] * s[i]) >> 7;
        out[i] = v > 255 ? 255 : v;
    }
}
//@LABS-STUB
inline void modulate_rgb(const PrimCtx& ctx, uint16_t texel, int out[3]) {
    // TODO(5): modulate expanded texel components by the primitive shade.
    (void)ctx;
    (void)texel;
    out[0] = out[1] = out[2] = 0;
}
//@LABS-END

// Stage: 24bit->15bit dithering. Offsets from the PSX-SPX 4x4 table are
// added to the 8-bit components, the sum saturates to 00h..FFh, and only
// then truncates to 5 bits (>>3). Never applied to rectangles.
//@LABS-BEGIN 6
//@LABS-SOLUTION
inline void dither_apply(int rgb[3], int x, int y, bool enabled) {
    if (!enabled) return;
    for (int i = 0; i < 3; ++i) {
        int v = rgb[i] + kDither[y & 3][x & 3];
        v = v < 0 ? 0 : (v > 255 ? 255 : v);
        rgb[i] = v;
    }
}
//@LABS-STUB
inline void dither_apply(int rgb[3], int x, int y, bool enabled) {
    // TODO(6): add the kDither[y&3][x&3] offset with saturation.
    (void)x;
    (void)y;
    (void)enabled;
    (void)rgb;
}
//@LABS-END

// Stage: semi-transparent blend of backdrop B and front F, per 5-bit
// component, saturated. Mode index comes from Texpage bits 5-6.
//@LABS-BEGIN 7
//@LABS-SOLUTION
inline uint16_t semi_blend(uint16_t bg, uint16_t fg, int mode) {
    const Rgb5 b = unpack_bgr15(bg);
    const Rgb5 f = unpack_bgr15(fg);
    auto eq = [mode](int bv, int fv) -> int {
        switch (mode) {
            case 0: return (bv + fv) / 2;        // B/2 + F/2
            case 1: return bv + fv;              // B + F
            case 2: return bv - fv;              // B - F
            default: return bv + fv / 4;         // B + F/4
        }
    };
    return pack_bgr15(eq(b.r, f.r), eq(b.g, f.g), eq(b.b, f.b));
}
//@LABS-STUB
inline uint16_t semi_blend(uint16_t bg, uint16_t fg, int mode) {
    // TODO(7): apply the four PSX-SPX semi-transparency equations.
    (void)mode;
    return static_cast<uint16_t>(((bg / 2) & fg) & 0x7FFF);
}
//@LABS-END

// ---------------------------------------------------------------------------
// Shared-device interface for this exercise. Texture fetch is plain 15bpp
// direct colour (with window wrapping), so every graded rule above is
// observable in isolation.
// ---------------------------------------------------------------------------
struct BlendClipStages {
    static int screen_coord(int v, int offset) { return offset_coord(v, offset); }

    static bool in_draw_area(int x, int y, const DrawArea& a) {
        return clip_in_draw_area(x, y, a);
    }

    static uint16_t texture_fetch(const TexEnv& env, int uf8, int vf8) {
        const Vram& vram = *env.vram;
        const TexWindow& w = env.win;
        const int u = (((uf8 >> 8) & ~(w.mask_x * 8)) |
                       ((w.off_x & w.mask_x) * 8)) & 0xFF;
        const int v = (((vf8 >> 8) & ~(w.mask_y * 8)) |
                       ((w.off_y & w.mask_y) * 8)) & 0x1FF;
        const int page_x = env.mode.page_x_field * 64;
        return vram.px[page_x + v * kVramWidth + u];  // 15bpp direct colour
    }

    static bool transparency_skip(uint16_t texel, const PrimCtx&) {
        return texel_transparent(texel);
    }

    static bool mask_test(uint16_t bg, const PrimCtx& ctx) {
        return mask_blocks(bg, ctx.mask);
    }

    static uint16_t blend_pixel(uint16_t bg, uint16_t texel,
                                const PrimCtx& ctx, int x, int y) {
        uint16_t fg;
        if (!ctx.textured) {
            fg = ctx.shade15;  // monochrome colour, never dithered here
        } else if (ctx.raw) {
            fg = texel;        // decal/raw texture: no modulation, no dither
        } else {
            int rgb[3];
            modulate_rgb(ctx, texel, rgb);
            dither_apply(rgb, x, y, ctx.dither);
            fg = pack_bgr15(rgb[0] >> 3, rgb[1] >> 3, rgb[2] >> 3);
        }
        return ctx.semi ? semi_blend(bg, fg, ctx.semi_mode) : fg;
    }
};

}  // namespace psx::gpu
