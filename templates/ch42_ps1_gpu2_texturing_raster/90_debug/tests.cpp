#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>

#include "debug_stages.hpp"

using namespace psx::gpu;

namespace {

// VRAM with one 4bpp page at field 0 and an odd-field twin at 64 halfwords,
// plus a 16-entry CLUT at (0,100) with entry i = 0x111*(i+1).
struct Fixture {
    Vram vram;
    DrawMode even;
    DrawMode odd;
    Clut clut{0, 100};

    Fixture() {
        for (int i = 0; i < 16; ++i)
            vram.at(i, 100) = static_cast<uint16_t>(0x111 * (i + 1));
        vram.px[0] = 0x7654;   // lanes 4,5,6,7
        vram.px[64] = 0x7654;  // same data on the odd page
        even.page_x_field = 0;
        odd.page_x_field = 1;
        even.depth = odd.depth = 0;
    }
};

}  // namespace

// REGRESSION (Bug A): odd texture-page X must mirror nibble lanes.
TEST(debug, regression_odd_page_lane_flip) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    // Even page: u=0 reads lane 0 -> entry 4.
    EXPECT_EQ(detail::debug_fetch(f.vram, f.even, f.clut, 0, 0), 0x111 * 5);
    // Odd page: u=0 must read lane 3 -> entry 7.
    EXPECT_EQ(detail::debug_fetch(f.vram, f.odd, f.clut, 0, 0), 0x111 * 8);
}

// REGRESSION (Bug A): same quirk on 8bpp byte order.
TEST(debug, regression_odd_page_byte_flip_8bpp) {
    auto f_storage = std::make_unique<Fixture>();
    Fixture& f = *f_storage;
    f.clut.y = 101;
    for (int i = 0; i < 256; ++i)
        f.vram.at(i, 101) = static_cast<uint16_t>(0x0101 * (i + 1));
    DrawMode m{};
    m.depth = 1;
    m.page_x_field = 1;
    f.vram.px[64] = 0xAB12;
    // Flipped: u=0 reads the HIGH byte (0xAB = 171).
    const uint16_t got = detail::debug_fetch(f.vram, m, f.clut, 0, 0);
    EXPECT_EQ(got, 0x0101 * 172);
    (void)f.odd;
}

// REGRESSION (Bug B): drawing area bottom/right corners are inclusive.
TEST(debug, regression_clip_corner_inclusive) {
    const DrawArea a{10, 20, 99, 199};
    EXPECT_TRUE(DebugStages::in_draw_area(99, 150, a));   // column X2
    EXPECT_TRUE(DebugStages::in_draw_area(50, 199, a));   // row Y2
    EXPECT_TRUE(DebugStages::in_draw_area(99, 199, a));   // corner
    EXPECT_FALSE(DebugStages::in_draw_area(100, 150, a)); // beyond stays out
    EXPECT_FALSE(DebugStages::in_draw_area(50, 200, a));
}

// Sanity (green on correct AND buggy builds): untouched stages behave.
TEST(debug, sanity_blend_and_transparency) {
    PrimCtx ctx;
    ctx.textured = true;
    ctx.semi = true;
    ctx.semi_mode = 2;  // B - F
    ctx.shade_r = ctx.shade_g = ctx.shade_b = 128;
    // Backdrop white minus full-red front: green/blue keep 31, red drops.
    const uint16_t out =
        DebugStages::blend_pixel(0x7FFF, pack_bgr15(31, 0, 0), ctx, 0, 3);
    EXPECT_EQ(out, pack_bgr15(0, 31, 31));
    EXPECT_TRUE(DebugStages::transparency_skip(0x0000, ctx));
    EXPECT_FALSE(DebugStages::transparency_skip(0x8000, ctx));
}
