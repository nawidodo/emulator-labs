#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>

#include "tex_stages.hpp"

using namespace psx::gpu;

namespace {

// Builds a tiny VRAM with one 4bpp texture at page (0,0) and a 16-entry CLUT
// at (0,100): entry i has the distinctive colour 0x111*(i+1).
struct Fixture {
    Vram vram;
    DrawMode mode;
    Clut clut{0, 100};

    Fixture() {
        for (int i = 0; i < 16; ++i)
            vram.at(i, 100) = static_cast<uint16_t>(0x111 * (i + 1));
        // Page row 0: texels 7,6,5,4 | 3,2,1,0 (lane 0 is the leftmost).
        vram.px[0] = 0x7654;
        vram.px[1] = 0x3210;
        mode.page_x_field = 0;
        mode.page_y_base = 0;
        mode.depth = 0;
    }
};

}  // namespace

TEST(pages, decode_draw_mode_fields) {
    DrawMode m;
    decode_draw_mode(0xE1000000 | 0x152u, m);  // field bits: 0b1_0101_0010
    EXPECT_EQ(m.page_x_field, 2);
    EXPECT_EQ(m.page_y_base, 256);             // bit4 selects the Y=256 page
    EXPECT_EQ(m.semi, 2);
    EXPECT_EQ(m.depth, 2);
    EXPECT_FALSE(m.dither);
}

TEST(pages, decode_clut_attribute) {
    // X in 16-halfword steps in bits 16-21, Y in bits 22-28.
    const uint32_t w = (320 / 16u) << 16 | (480u << 22);
    const Clut c = decode_clut(w);
    EXPECT_EQ(c.x, 320);
    EXPECT_EQ(c.y, 480);
}

TEST(pages, window_wrap_identity_when_mask_zero) {
    TexWindow w;  // all zero: window covers the full page
    EXPECT_EQ(wrap_u(0, w), 0);
    EXPECT_EQ(wrap_u(255, w), 255);
    EXPECT_EQ(wrap_u(42, w), 42);
    EXPECT_EQ(wrap_v(300, w), 300);
}

TEST(pages, window_wrap_repeat_period) {
    // Mask field 1 masks bit 3 -> coordinates repeat every 16 texels.
    TexWindow w{};
    w.mask_x = 1;
    w.off_x = 0;
    EXPECT_EQ(wrap_u(10, w), 2);   // 1010b & ~01000b = 0010b
    EXPECT_EQ(wrap_u(17, w), 17);  // bit3 clear: untouched by the mask
    w.off_x = 1;                   // offset re-inserts bit 3
    EXPECT_EQ(wrap_u(2, w), 10);   // 0010b | 01000b = 1010b
    EXPECT_EQ(wrap_u(18, w), 26);  // 10010b | 01000b = 11010b
}

TEST(pages, fetch_4bpp_low_nibble_is_leftmost) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 0, 0), 0x111 * 5);
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 1, 0), 0x111 * 6);
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 6, 0), 0x111 * 3);
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 7, 0), 0x111 * 4);
}

TEST(pages, fetch_4bpp_row_stride_is_one_vram_line) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.vram.at(0, 1) = 0x000F;  // first halfword of page row 1 = next VRAM line
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 0, 1), 0x111 * 16);
}

TEST(pages, fetch_4bpp_odd_page_flips_lanes) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.mode.page_x_field = 1;             // base X = 64 halfwords (odd field)
    f.vram.px[64] = 0x7654;
    // Lane order mirrors: u=0 now reads nibble 3.
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 0, 0), 0x111 * 8);
    EXPECT_EQ(fetch_texel_4bpp(f.vram, f.mode, f.clut, 3, 0), 0x111 * 5);
}

TEST(pages, fetch_8bpp_byte_lanes) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.mode.depth = 1;
    f.clut.y = 101;
    for (int i = 0; i < 256; ++i)
        f.vram.at(i, 101) = static_cast<uint16_t>(0x0101 * (i + 1));
    f.vram.px[0] = 0xAB12;  // low byte = leftmost texel
    EXPECT_EQ(fetch_texel_8bpp(f.vram, f.mode, f.clut, 0, 0), 0x0101 * 19);
    EXPECT_EQ(fetch_texel_8bpp(f.vram, f.mode, f.clut, 1, 0), 0x0101 * 172);
}

TEST(pages, fetch_8bpp_odd_page_flips_bytes) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.mode.depth = 1;
    f.mode.page_x_field = 1;  // odd field: byte lanes swap
    f.clut.y = 101;
    for (int i = 0; i < 256; ++i)
        f.vram.at(i, 101) = static_cast<uint16_t>(0x0101 * (i + 1));
    f.vram.px[64] = 0xAB12;
    EXPECT_EQ(fetch_texel_8bpp(f.vram, f.mode, f.clut, 0, 0), 0x0101 * 172);
    EXPECT_EQ(fetch_texel_8bpp(f.vram, f.mode, f.clut, 1, 0), 0x0101 * 19);
}

TEST(pages, fetch_15bpp_direct) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.mode.depth = 2;
    f.mode.page_x_field = 2;  // base X = 128 halfwords
    f.vram.at(128 + 3, 0) = 0x7BCD;
    f.vram.at(128 + 1, 2) = 0x1234;   // page row 2 lives two VRAM lines down
    EXPECT_EQ(fetch_texel_15bpp(f.vram, f.mode, 3, 0), 0x7BCD);
    EXPECT_EQ(fetch_texel_15bpp(f.vram, f.mode, 1, 2), 0x1234);
}

TEST(pages, stages_fetch_integration) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    TexEnv env;
    env.win = TexWindow{};
    env.vram = &f.vram;
    env.clut = f.clut;
    // uf8/vf8 are 8.8 fixed point as produced by the rasterizer.
    EXPECT_EQ(TexPagesStages::texture_fetch(env, 1 << 8, 0), 0x111 * 6);
}

TEST(pages, transparency_rule_black_vs_stp) {
    PrimCtx ctx;
    EXPECT_TRUE(TexPagesStages::transparency_skip(0x0000, ctx));
    EXPECT_FALSE(TexPagesStages::transparency_skip(0x8000, ctx));
    EXPECT_FALSE(TexPagesStages::transparency_skip(0x0421, ctx));
}

TEST(pages, mask_test_honours_e6h) {
    PrimCtx ctx;
    ctx.mask.test_bit = false;
    EXPECT_FALSE(TexPagesStages::mask_test(0xFFFF, ctx));
    ctx.mask.test_bit = true;
    EXPECT_TRUE(TexPagesStages::mask_test(0x8000, ctx));
    EXPECT_FALSE(TexPagesStages::mask_test(0x7FFF, ctx));
}

TEST(pages, clip_bounds_inclusive) {
    DrawArea a{10, 20, 99, 199};
    EXPECT_TRUE(TexPagesStages::in_draw_area(10, 20, a));    // top-left corner
    EXPECT_TRUE(TexPagesStages::in_draw_area(99, 199, a));   // bottom-right corner
    EXPECT_FALSE(TexPagesStages::in_draw_area(100, 199, a));  // past right edge
    EXPECT_FALSE(TexPagesStages::in_draw_area(10, 200, a));   // past bottom edge
}
