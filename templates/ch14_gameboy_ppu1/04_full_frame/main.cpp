#define LABSTEST_MAIN
#include <cstdio>
#include <cstring>

#include "labstest.hpp"
#include "ppu.hpp"

namespace {

// Build a tiny snapshot in memory: tile 1 = left/right halves, row 0 of
// the map filled with it, default BGP.
gbppu::PpuState makeState() {
    gbppu::PpuState s{};
    std::memset(&s, 0, sizeof(s));
    for (int y = 0; y < 8; ++y) {
        s.vram[16 + 2 * y] = 0xFF;      // tile 1 plane0: all index 1
        s.vram[17 + 2 * y] = 0x00;
    }
    for (int i = 0; i < 32 * 32; ++i) s.vram[0x1800 + i] = 1;
    s.lcdc = 0x91;
    s.bgp = 0xE4;
    return s;
}

}  // namespace

TEST(frame, lcd_off_is_white) {
    gbppu::PpuState s = makeState();
    s.lcdc &= ~gbppu::kLcdcLcdOn;
    static gbppu::Frame f;
    gbppu::renderFrame(s, f);
    EXPECT_EQ(f[0][0][0], 255);  // white everywhere: shade 0 ramp
    EXPECT_EQ(f[143][159][3], 255);
}

TEST(frame, full_frame_dimensions_and_content) {
    gbppu::PpuState s = makeState();
    static gbppu::Frame f;
    gbppu::renderFrame(s, f);
    // Tile 1 is uniformly color index 1 -> BGP shade 1 -> gray 192.
    EXPECT_EQ(f[10][0][0], 192);
    EXPECT_EQ(f[10][4][0], 192);
    // Every visible line identical (no scroll).
    EXPECT_EQ(std::memcmp(f[0], f[100], 160 * 4), 0);
}

TEST(frame, loadState_rejects_bad_files) {
    gbppu::PpuState s{};
    EXPECT_FALSE(gbppu::loadState("/nonexistent/snapshot.ppu", s));

    FILE* tmp = std::fopen("short_snapshot.tmp", "wb");
    std::fputc(0, tmp);
    std::fclose(tmp);
    EXPECT_FALSE(gbppu::loadState("short_snapshot.tmp", s));
    std::remove("short_snapshot.tmp");
}
