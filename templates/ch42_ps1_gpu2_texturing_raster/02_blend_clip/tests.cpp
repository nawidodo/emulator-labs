#define LABSTEST_MAIN
#include "labstest.hpp"

#include "blend_stages.hpp"

using namespace psx::gpu;

TEST(blend, offset_added_before_clip) {
    EXPECT_EQ(offset_coord(-5, 15), 10);
    const DrawArea a{10, 20, 99, 199};
    // The shifted coordinate is what the clip test must see.
    const int sx = offset_coord(-5, 15);
    EXPECT_TRUE(clip_in_draw_area(sx, 20, a));
}

TEST(blend, draw_area_inclusive_both_ends) {
    const DrawArea a{10, 20, 99, 199};
    EXPECT_TRUE(clip_in_draw_area(10, 20, a));
    EXPECT_TRUE(clip_in_draw_area(99, 199, a));   // bottom-right INCLUDED
    EXPECT_FALSE(clip_in_draw_area(100, 150, a));
    EXPECT_FALSE(clip_in_draw_area(50, 200, a));
    EXPECT_FALSE(clip_in_draw_area(9, 20, a));
}

TEST(blend, transparency_rule_stp_bit) {
    EXPECT_TRUE(texel_transparent(0x0000));   // all colour bits zero
    EXPECT_FALSE(texel_transparent(0x8000));  // STP set: drawable black
    EXPECT_FALSE(texel_transparent(0x0001));
    EXPECT_FALSE(texel_transparent(0x7FFF));
}

TEST(blend, mask_test_blocks_on_bit15) {
    MaskSetting m;
    m.test_bit = true;
    EXPECT_TRUE(mask_blocks(0x8000, m));
    EXPECT_FALSE(mask_blocks(0x7FFF, m));
    m.test_bit = false;
    EXPECT_FALSE(mask_blocks(0xFFFF, m));
}

TEST(blend, modulate_identity_at_shade_80h) {
    PrimCtx ctx;
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 128;
    int rgb[3];
    modulate_rgb(ctx, 0x7FFF, rgb);
    EXPECT_EQ(rgb[0], 255);
    EXPECT_EQ(rgb[1], 255);
    EXPECT_EQ(rgb[2], 255);
}

TEST(blend, modulate_scales_and_saturates) {
    PrimCtx ctx;
    int rgb[3];
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 64;  // half brightness
    modulate_rgb(ctx, 0x7FFF, rgb);
    EXPECT_EQ(rgb[0], 127);  // (255*64)>>7
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 255;  // overdrive -> saturate
    modulate_rgb(ctx, 0x7FFF, rgb);
    EXPECT_EQ(rgb[0], 255);  // min(255, (255*255)>>7)
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 0;
    modulate_rgb(ctx, 0x7FFF, rgb);
    EXPECT_EQ(rgb[0], 0);
}

TEST(blend, dither_offsets_and_saturation) {
    int rgb[3] = {10, 10, 10};
    dither_apply(rgb, 0, 0, true);  // table[-4]
    EXPECT_EQ(rgb[0], 6);
    int g2[3] = {10, 10, 10};
    dither_apply(g2, 3, 0, true);  // +1
    EXPECT_EQ(g2[0], 11);
    int g3[3] = {10, 10, 10};
    dither_apply(g3, 1, 3, true);  // row 3 col 1 = -1
    EXPECT_EQ(g3[0], 9);
    int g4[3] = {2, 2, 2};
    dither_apply(g4, 0, 0, true);  // saturates at zero
    EXPECT_EQ(g4[0], 0);
    int g5[3] = {42, 42, 42};
    dither_apply(g5, 0, 0, false);  // disabled: untouched
    EXPECT_EQ(g5[0], 42);
}

TEST(blend, semi_equations) {
    const uint16_t black = 0x0000;
    const uint16_t white = 0x7FFF;
    EXPECT_EQ(semi_blend(white, black, 0), 0x3DEF);  // halves -> 15/15/15
    EXPECT_EQ(semi_blend(white, white, 1), 0x7FFF);  // saturating add
    const uint16_t r3 = pack_bgr15(3, 0, 0);
    const uint16_t r4 = pack_bgr15(4, 0, 0);
    EXPECT_EQ(semi_blend(r3, r4, 1), pack_bgr15(7, 0, 0));
    EXPECT_EQ(semi_blend(r3, r4, 2), pack_bgr15(0, 0, 0));  // clamps below 0
    EXPECT_EQ(semi_blend(black, white, 3), pack_bgr15(7, 7, 7));  // F/4
}

TEST(blend, pixel_pipeline_modulate_vs_decal) {
    Vram vram;
    vram.at(5, 5) = 0x03E0;  // green backdrop
    TexEnv env;
    env.mode.depth = 2;
    env.vram = &vram;

    PrimCtx mod;
    mod.textured = true;
    mod.shade_r = mod.shade_g = mod.shade_b = 128;
    // Modulated white texel over green backdrop stays white.
    EXPECT_EQ(BlendClipStages::blend_pixel(vram.at(5, 5), 0x7FFF, mod, 5, 5),
              0x7FFF);

    PrimCtx raw = mod;
    raw.raw = true;
    // Decal ignores the backdrop entirely and copies the texel.
    EXPECT_EQ(BlendClipStages::blend_pixel(vram.at(5, 5), 0x001F, raw, 5, 5),
              0x001F);
}

TEST(blend, pixel_pipeline_semi_transparent) {
    PrimCtx ctx;
    ctx.textured = true;
    ctx.semi = true;
    ctx.semi_mode = 0;  // B/2+F/2
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 128;
    // Half-intensity green texel blended onto black.
    EXPECT_EQ(BlendClipStages::blend_pixel(0x0000, 0x03E0, ctx, 0, 0),
              0x01E0);
}
