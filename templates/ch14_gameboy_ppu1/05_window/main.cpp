#define LABSTEST_MAIN
#include <cstdio>
#include <cstring>

#include "labstest.hpp"
#include "win.hpp"

namespace {

// BG tile 1: shade index 1 everywhere. Window alternates per map row:
// even content rows use tile 2 (index 3 -> black), odd rows tile 3
// (index 0 -> white) so window content lines are visually distinct.
gbwin::PpuState makeScene() {
    gbwin::PpuState s{};
    std::memset(&s, 0, sizeof(s));
    for (int y = 0; y < 8; ++y) {
        s.vram[16 + 2 * y] = 0xFF;   // tile 1: index 1
        s.vram[32 + 2 * y] = 0x00;   // tile 2: index 3
        s.vram[33 + 2 * y] = 0xFF;
        s.vram[48 + 2 * y] = 0x00;   // tile 3: index 0
        s.vram[49 + 2 * y] = 0x00;
    }
    for (int i = 0; i < 32 * 32; ++i) s.vram[0x1800 + i] = 1;   // BG
    for (int row = 0; row < 32; ++row)
        for (int col = 0; col < 32; ++col)
            s.vram[0x1C00 + row * 32 + col] = (row % 2 == 0) ? 2 : 3;
    s.lcdc = 0xF1;
    s.bgp = 0xE4;
    s.wy = 8;
    s.wx = 7 + 16;
    return s;
}

}  // namespace

TEST(win, map_base_selection) {
    EXPECT_EQ(gbwin::winMapBase(0x00), 0x1800);
    EXPECT_EQ(gbwin::winMapBase(0x40), 0x1C00);
}

TEST(win, active_only_below_wy_and_enabled) {
    gbwin::PpuState s = makeScene();
    EXPECT_FALSE(gbwin::windowActive(s, 7));
    EXPECT_TRUE(gbwin::windowActive(s, 8));
    EXPECT_TRUE(gbwin::windowActive(s, 143));
    s.lcdc &= ~gbwin::kLcdcWinEnable;
    EXPECT_FALSE(gbwin::windowActive(s, 100));
}

TEST(win, window_overwrites_bg_region_only) {
    gbwin::PpuState s = makeScene();
    static gbwin::Frame f;
    gbwin::renderFrame(s, f);
    // Window tiles: even map rows use tile #2 (color index 2 -> gray
    // 96), odd rows use tile #3 (index 0 -> white).
    // Line 20 -> content row 12 -> map row 1 (odd -> white);
    // x<15 stays BG gray.
    EXPECT_EQ(f[20][15][0], 192);
    EXPECT_EQ(f[20][16][0], 255);
    // Line 30 -> content row 22 -> map row 2 (even -> gray 96).
    EXPECT_EQ(f[30][30][0], 96);
}

TEST(win, internal_line_counter_skips_disabled_lines) {
    gbwin::PpuState s = makeScene();
    static gbwin::Frame f;
    gbwin::renderFrame(s, f);

    // Re-render but hold the window disabled for screen lines 50..58
    // (nine lines). The content must NOT restart from WY afterwards:
    // screen line 59 draws content row 42 (42 rows drawn so far), not
    // the continuous frame's row 51.
    static gbwin::Frame g;
    const uint8_t lcdcOn = s.lcdc;
    int wl = 0;
    for (int ly = 0; ly < 144; ++ly) {
        const bool winOff = (ly >= 50 && ly < 59);
        s.lcdc = winOff ? static_cast<uint8_t>(
                              lcdcOn & ~gbwin::kLcdcWinEnable)
                        : lcdcOn;
        gbwin::renderScanline(s, ly, wl, g[ly]);
        if (gbwin::windowActive(s, ly)) ++wl;
    }
    // Continuous line 59: content row 51 -> map row 6 (even -> gray 96).
    // Toggled line 59: content row 42 -> map row 5 (odd -> white).
    EXPECT_EQ(f[59][20][0], 96);
    EXPECT_EQ(g[59][20][0], 255);
}
