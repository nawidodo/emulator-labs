#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "ppu.hpp"

using namespace gba;

using gba::PpuMemory;

TEST(window, masks_by_rectangle_and_fallback) {
    PpuMemory m;
    // WIN0: x 10..19, y 20..39, mask = BG0|OBJ (0b010001 -> 0x11).
    m.wr16(0x04000040, u16(10 | 20 << 8));
    m.wr16(0x04000042, u16(20 | 40 << 8));
    m.wr16(0x04000048, 0x0011);
    EXPECT_EQ(window_mask(m, 15, 30), 0x11);
    // Outside both windows -> WINOUT low byte.
    m.wr16(0x0400004A, 0x003F);
    EXPECT_EQ(window_mask(m, 200, 100), 0x3F);
    // WIN1 covers x 100..149 via its own rect and mask.
    m.wr16(0x04000044, u16(100 | 150 << 8));
    m.wr16(0x04000046, 0x0000);  // y 0..? start>end would disable; use full
    m.wr16(0x04000046, u16(0 | 160 << 8));
    m.io[0x49] = 0x04;  // WININ high byte: BG2 only
    EXPECT_EQ(window_mask(m, 120, 50), 0x04);
}

TEST(mosaic, snaps_to_block_origins) {
    int x = 37, y = 55;
    mosaic_quantize(x, y, 7, 3);  // blocks of 8 and 4
    EXPECT_TRUE(x == 32 && y == 52);
    x = 5;
    y = 5;
    mosaic_quantize(x, y, 0, 0);  // disabled: block size 1
    EXPECT_TRUE(x == 5 && y == 5);
}

namespace {

// Mode-3 world: red backdrop, green bitmap, one blue 8x8 sprite.
void build_mode3_scene(PpuMemory& m) {
    m.wr16(PpuMemory::kIoBase, 0x0483);  // mode 3, BG2 show, OBJ show
    m.wr16(PpuMemory::kPalBase + 0, 0x001F);          // backdrop red
    // Green rectangle wide enough to sit under the sprite as well.
    for (int y = 60; y < 140; ++y)
        for (int x = 60; x < 200; ++x)
            m.wr16(0x06000000 + (u32(y) * 240 + u32(x)) * 2, 0x03E0);
    // Sprite 0: blue square at (120,80), tile 1, prio 1.
    m.oam[0] = 80;
    m.oam[1] = 0;
    m.oam[2] = 120 & 0xFF;
    m.oam[3] = u8((120 >> 8) & 1);  // size bits stay 0 -> 8x8
    m.oam[4] = 1;
    m.oam[5] = 1 << 2;              // priority 1
    for (int i = 0; i < 32; ++i) m.vram[kObjTileBase + 32 + i] = 0x1F;
    // OBJ palette entry for color index 15 of bank 0 -> blue.
    m.wr16(PpuMemory::kPalBase + (256 + 15) * 2, 0x7C00);
    // No windows in this scene: everything visible outside both.
    m.wr16(0x0400004A, 0x003F);
}

}  // namespace

TEST(picklayers, priority_order_obj_tie_and_bg_number) {
    LayerPix bgs[4];
    bgs[0] = {true, 1, 1, false, 0};
    bgs[1] = {true, 2, 1, false, 1};
    bgs[3] = {true, 3, 0, false, 3};
    LayerPix obj = {true, 4, 1, false, 4};
    LayerPix top, second;
    pick_layers(bgs, obj, top, second);
    EXPECT_EQ(top.layer_id, 3);     // priority 0 beats everything
    EXPECT_EQ(second.layer_id, 4);  // OBJ wins the priority-1 tie vs BG0/BG1
}

TEST(picklayers, transparent_layers_skipped) {
    LayerPix bgs[4];
    bgs[2] = {true, 9, 2, false, 2};
    LayerPix obj;
    LayerPix top, second;
    pick_layers(bgs, obj, top, second);
    EXPECT_EQ(top.layer_id, 2);
    EXPECT_FALSE(second.opaque);
}

TEST(bld, alpha_blend_clamps_coefficients) {
    PpuMemory m;
    m.wr16(0x04000050, u16(2 | 0x100 | 0x40));  // alpha, 1st=BG1, 2nd=BG0
    m.wr16(0x04000052, u16(31 | (0 << 8)));     // EVA clamps down to 16
    LayerPix top = {true, 0x7FFF, 0, false, 1};    // white
    LayerPix bottom = {true, 0x0000, 1, false, 0}; // black
    // EVB=0, EVA clamped to 16/16: pure top passes through.
    m.wr16(0x04000052, u16(16 | (0 << 8)));
    EXPECT_EQ(apply_bld(m, top, bottom, true), 0x7FFF);
    // Half-and-half of red over black.
    LayerPix redtop = {true, 0x001F, 0, false, 1};
    m.wr16(0x04000052, u16(8 | (8 << 8)));
    EXPECT_EQ(apply_bld(m, redtop, bottom, true), 0x000F);
}

TEST(bld, brightness_effects) {
    PpuMemory m;
    LayerPix mid = {true, u16(16 | 16 << 5 | 16 << 10), 0, false, 0};
    m.wr16(0x04000050, 0x0081);   // mode 2 lighten, first target BG0
    m.wr16(0x04000054, 16);       // full intensity -> white
    u16 c = apply_bld(m, mid, LayerPix{}, true);
    EXPECT_EQ(c & 31u, 31u);
    m.wr16(0x04000050, 0x00C1);   // mode 3 darken
    c = apply_bld(m, mid, LayerPix{}, true);
    EXPECT_EQ(c & 31u, 0u);
}

TEST(scene, mode3_sprite_priority_and_determinism) {
    PpuMemory m;
    build_mode3_scene(m);
    std::vector<u32> frame(kScreenW * kScreenH);
    compose_frame(m, frame.data());
    // Mode 3 has no transparency: unwritten VRAM is opaque black.
    EXPECT_EQ(frame[0], bgr555_to_rgba8888(0x0000));
    // Bitmap-only region.
    EXPECT_EQ(frame[65 * kScreenW + 65], bgr555_to_rgba8888(0x03E0));
    // Sprite overlaps bitmap at (120..127, 80..87) with prio 1 vs BG prio 0:
    // background wins because its priority value is LOWER.
    EXPECT_EQ(frame[82 * kScreenW + 122], bgr555_to_rgba8888(0x03E0));

    // Raise the sprite to priority 0: it now beats the bitmap.
    m.oam[5] = 0;
    compose_frame(m, frame.data());
    EXPECT_EQ(frame[82 * kScreenW + 122], bgr555_to_rgba8888(0x7C00));  // blue

    std::vector<u32> again(kScreenW * kScreenH);
    compose_frame(m, again.data());
    EXPECT_EQ(fnv64(frame.data(), frame.size() * 4),
              fnv64(again.data(), again.size() * 4));
}

TEST(scene, window_hides_layer) {
    PpuMemory m;
    build_mode3_scene(m);
    // Window 0 covering the whole screen but masking OUT BG2.
    m.wr16(0x04000040, u16(0 | 240 << 8));
    m.wr16(0x04000042, u16(0 | 160 << 8));
    m.wr16(0x04000048, 0x0011);  // only BG0+OBJ visible inside
    m.wr16(0x0400004A, 0x003F);  // everything visible outside (unused)
    std::vector<u32> frame(kScreenW * kScreenH);
    compose_frame(m, frame.data());
    // Bitmap masked out inside the window -> backdrop red shows instead.
    EXPECT_EQ(frame[65 * kScreenW + 65], bgr555_to_rgba8888(0x001F));
}
