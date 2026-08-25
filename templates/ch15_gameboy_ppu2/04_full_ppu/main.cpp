// Tests for exercise 04: the complete scanline renderer.
#define LABSTEST_MAIN
#include <cstring>

#include "labstest.hpp"
#include "ppu.hpp"

namespace {

using gbppu2::PpuState;

PpuState baseState() {
    PpuState s{};
    s.lcdc = gbppu2::kLcdcLcdOn | gbppu2::kLcdcTileUnsigned |
             gbppu2::kLcdcSpritesEnable | gbppu2::kLcdcBgEnable;
    s.bgp = 0xE4;   // identity
    s.obp0 = 0xE4;  // identity
    s.obp1 = 0x1B;  // inverted fields
    return s;
}

void putSolidTile(PpuState& s, uint8_t tile, uint8_t idx) {
    const uint8_t lo = idx & 1 ? 0xFF : 0x00;
    const uint8_t hi = idx & 2 ? 0xFF : 0x00;
    for (int r = 0; r < 8; ++r) {
        s.vram[tile * 16 + 2 * r] = lo;
        s.vram[tile * 16 + 2 * r + 1] = hi;
    }
}

void putStripeTile(PpuState& s, uint8_t tile, bool even) {
    // Vertical stripes: index 3 on even columns when `even`, odd otherwise.
    std::memset(&s.vram[tile * 16], 0, 16);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if ((c % 2 == 0) == even) {
                s.vram[tile * 16 + 2 * r] |= 0x80 >> c;
                s.vram[tile * 16 + 2 * r + 1] |= 0x80 >> c;
            }
}

void fillMap(PpuState& s, uint16_t mapBase, uint8_t entry) {
    // The $9C00 map ends at the top of VRAM; never overwrite past it.
    const size_t n = static_cast<size_t>(0x2000 - mapBase);
    std::memset(&s.vram[mapBase], entry, n);
}

uint8_t shadeAt(const uint8_t rgba[160][4], int x) {
    for (int sh = 0; sh < 4; ++sh)
        if (rgba[x][0] == gbppu2::kGray[sh]) return static_cast<uint8_t>(sh);
    return 99;
}

}  // namespace

TEST(bgwin, scroll_fetch_and_bgp_shading) {
    PpuState s = baseState();
    putSolidTile(s, 1, 1);
    fillMap(s, 0x1800, 1);
    s.scx = 103;
    s.scy = 57;
    uint8_t idx[160] = {};
    uint8_t shd[160] = {};
    gbppu2::renderBgWindowScanline(s, 40, false, -1, idx, shd);
    for (int x = 0; x < 160; ++x) {
        EXPECT_EQ(idx[x], 1);
        EXPECT_EQ(shd[x], 1);  // BGP field of index 1 under identity BGP
    }
}
TEST(bgwin, window_overwrite_uses_internal_line) {
    PpuState s = baseState();
    s.lcdc |= gbppu2::kLcdcWinEnable | gbppu2::kLcdcWinMapHi;
    putSolidTile(s, 1, 0);      // BG tile: white
    putStripeTile(s, 2, true);  // window tile: index 3 on even columns
    fillMap(s, 0x1800, 1);
    fillMap(s, 0x1C00, 2);
    s.wy = 32;
    s.wx = 7 + 40;  // window starts at screen x=40

    uint8_t idx[160] = {};
    uint8_t shd[160] = {};
    gbppu2::renderBgWindowScanline(s, 33, true, 1, idx, shd);
    for (int x = 0; x < 40; ++x) {
        EXPECT_EQ(idx[x], 0);  // BG untouched left of the window
        EXPECT_EQ(shd[x], 0);
    }
    bool sawWindowPixel = false;
    for (int x = 40; x < 160; ++x) {
        EXPECT_TRUE(idx[x] == 0 || idx[x] == 3);  // window tile content
        if (idx[x] == 3) sawWindowPixel = true;
    }
    EXPECT_TRUE(sawWindowPixel);
}

TEST(fullframe, lcd_off_is_white) {
    PpuState s = baseState();
    putSolidTile(s, 1, 3);
    fillMap(s, 0x1800, 1);
    s.oam[0] = 100;  // a sprite that would be dark
    s.oam[1] = 100;
    s.oam[2] = 1;
    s.lcdc &= ~gbppu2::kLcdcLcdOn;
    gbppu2::Frame frame{};
    bool win[144] = {};
    gbppu2::renderFrame(s, frame, win);
    for (int y = 0; y < 144; y += 13)
        for (int x = 0; x < 160; x += 11) {
            EXPECT_EQ(frame[y][x][0], 255);
            EXPECT_EQ(frame[y][x][3], 255);
        }
}

TEST(fullframe, deterministic_across_repeats) {
    PpuState s = baseState();
    putSolidTile(s, 1, 1);
    putSolidTile(s, 2, 3);
    fillMap(s, 0x1800, 1);
    s.oam[0] = 70; s.oam[1] = 50; s.oam[2] = 2; s.oam[3] = 0;
    s.scx = 5;
    s.scy = 9;
    gbppu2::Frame a{};
    gbppu2::Frame b{};
    bool win[144] = {};
    gbppu2::renderFrame(s, a, win);
    gbppu2::renderFrame(s, b, win);
    EXPECT_EQ(std::memcmp(a, b, sizeof(a)), 0);
}

TEST(fullframe, sprite_visible_over_background) {
    PpuState s = baseState();
    putSolidTile(s, 1, 1);
    putSolidTile(s, 2, 3);
    fillMap(s, 0x1800, 1);
    s.oam[0] = 70; s.oam[1] = 50; s.oam[2] = 2; s.oam[3] = 0;
    gbppu2::Frame frame{};
    bool win[144] = {};
    gbppu2::renderFrame(s, frame, win);
    // Sprite covers lines 54..61, columns 42..49.
    EXPECT_EQ(shadeAt(frame[58], 45), 3);
    EXPECT_EQ(shadeAt(frame[58], 41), 1);
    EXPECT_EQ(shadeAt(frame[53], 45), 1);   // above sprite
    EXPECT_EQ(shadeAt(frame[62], 45), 1);   // below sprite
}

TEST(fullframe, sprites_disabled_bit_hides_layer) {
    PpuState s = baseState();
    putSolidTile(s, 1, 1);
    putSolidTile(s, 2, 3);
    fillMap(s, 0x1800, 1);
    s.oam[0] = 70; s.oam[1] = 50; s.oam[2] = 2; s.oam[3] = 0;
    s.lcdc &= ~gbppu2::kLcdcSpritesEnable;
    gbppu2::Frame frame{};
    bool win[144] = {};
    gbppu2::renderFrame(s, frame, win);
    EXPECT_EQ(shadeAt(frame[58], 45), 1);
}

TEST(windowtoggle, disabled_lines_skip_content_rows) {
    PpuState s = baseState();
    s.lcdc |= gbppu2::kLcdcWinEnable | gbppu2::kLcdcWinMapHi;
    putSolidTile(s, 1, 0);
    putSolidTile(s, 2, 3);
    putSolidTile(s, 3, 1);
    fillMap(s, 0x1800, 1);
    // Window map: tile rows 0-2 use tile 2 (dark), rows 3+ tile 3.
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 2;
    for (int r = 3; r < 32; ++r)
        for (int c = 0; c < 32; ++c) s.vram[0x1C00 + r * 32 + c] = 3;
    s.wy = 0;
    s.wx = 7;

    gbppu2::Frame frame{};
    bool win[144];
    for (bool& b : win) b = true;
    for (int ly = 40; ly < 48; ++ly) win[ly] = false;  // mid-frame toggle
    gbppu2::renderFrame(s, frame, win);

    // Content advances only while the window draws. Line 39 -> content 39;
    // after eight disabled lines, line 48 -> content 40.
    auto windowShadeAt = [&](int ly, int x) { return shadeAt(frame[ly], x); };
    // Line 16 is content line 16 -> tile row 2 (tile 2, dark).
    EXPECT_EQ(windowShadeAt(16, 80), 3);
    // Line 36 is content row 36/8=4 -> tile-row 4 (tile 3).
    EXPECT_EQ(windowShadeAt(36, 80), 1);
    // Line 48 must show content row 40/8=5 (tile 3): the eight skipped
    // screen lines did NOT advance the counter.
    EXPECT_EQ(windowShadeAt(48, 80), 1);
    // And line 88 sits in content row 88-8=80 -> row 10 (tile 3).
    EXPECT_EQ(windowShadeAt(88, 80), 1);
}
