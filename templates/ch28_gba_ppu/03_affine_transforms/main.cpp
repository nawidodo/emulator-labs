#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "affine.hpp"

using namespace gba;

namespace {

// 128x128 texture: tile t filled with color (t % 251) + 1.
// Screen map lives at 0x4000, safely beyond the 16 KiB tile pixel area.
void setup_texture(PpuMemory& m) {
    for (int ty = 0; ty < 16; ++ty)
        for (int tx = 0; tx < 16; ++tx) {
            u8 entry = u8(ty * 16 + tx);
            m.vram[0x4000 + ty * 16 + tx] = entry;
            u8 color = u8((entry % 251) + 1);
            for (int r = 0; r < 8; ++r)
                for (int c = 0; c < 8; ++c)
                    m.vram[u32(entry) * 64 + r * 8 + c] = color;
        }
}

AffineBgConfig small_cfg() {
    AffineBgConfig cfg;
    cfg.screen_base = 0x4000;
    cfg.size_log2 = 7;  // 128 texels
    return cfg;
}

}  // namespace

TEST(fixed, mul_keeps_fraction) {
    EXPECT_EQ(fixed_mul(256, 256), 256);   // 1.0 * 1.0
    EXPECT_EQ(fixed_mul(512, 128), 256);   // 2.0 * 0.5
    EXPECT_EQ(fixed_mul(-256, 256), -256); // sign preserved
}

TEST(params, decode_identity_and_offset) {
    PpuMemory m;
    m.wr16(PpuMemory::kIoBase + 0x20, 256);  // PA = 1.0
    m.wr16(PpuMemory::kIoBase + 0x26, 256);  // PD = 1.0
    m.wr16(PpuMemory::kIoBase + 0x28, 100);  // DX low half
    AffineParams p = AffineParams::decode(m, 2);
    EXPECT_EQ(p.pa, 256);
    EXPECT_EQ(p.pd, 256);
    EXPECT_EQ(p.dx, 100);
    // BG3 block mirrors at 0x30 and must not alias BG2.
    m.wr16(PpuMemory::kIoBase + 0x30, 512);
    EXPECT_EQ(AffineParams::decode(m, 3).pa, 512);
    EXPECT_EQ(AffineParams::decode(m, 2).pa, 256);
}

TEST(affine, latch_accumulates_vertical_matrix) {
    s32 x, y;
    latch_line(AffineParams{}, 0, x, y);
    EXPECT_TRUE(x == 0 && y == 0);
    // Results are 8.8 fixed point: one texel per line = 256 per line.
    latch_line(AffineParams{}, 8, x, y);
    EXPECT_TRUE(x == 0 && y == 2048);
    // PB=256 (one extra texel per line): x advances with the scanline too,
    // while PD stays identity so y advances equally.
    AffineParams q;
    q.pb = 256;
    latch_line(q, 4, x, y);
    EXPECT_TRUE(x == 1024 && y == 1024);
}

TEST(affine, identity_maps_texels_one_to_one) {
    PpuMemory m;
    setup_texture(m);
    int line[kScreenW];
    render_affine_scanline(m, small_cfg(), AffineParams{}, 10, line);
    // Screen (3,10) -> texel (3,10), tile row 1 col 0, entry 16.
    EXPECT_EQ(line[3], ((16 % 251) + 1));
}

TEST(affine, wrap_repeats_texture_seamlessly) {
    PpuMemory m;
    setup_texture(m);
    AffineParams p;
    p.dx = -5 << 8;  // shift left by five texels (8.8 fixed)
    int line[kScreenW];
    render_affine_scanline(m, small_cfg(), p, 130, line);  // y wraps past 128
    // Texel x = 118 (tile col 14), texel y = 130 & 127 = 2 -> entry 14.
    EXPECT_EQ(line[123], ((14 % 251) + 1));
}

TEST(affine, scale_zooms_by_two) {
    PpuMemory m;
    setup_texture(m);
    AffineParams p;
    p.pa = 512;  // 2.0: one texel becomes two screen pixels
    p.pd = 512;
    int line[kScreenW];
    render_affine_scanline(m, small_cfg(), p, 0, line);
    // Screen x=20 -> texel 40, tile col 5.
    EXPECT_EQ(line[20], ((5 % 251) + 1));
}
