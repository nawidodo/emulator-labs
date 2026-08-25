#define LABSTEST_MAIN
#include <cstdio>
#include <cstring>

#include "labstest.hpp"
#include "bg.hpp"

namespace {

// Tile 1: x<4 -> index 1 (plane 0), x>=4 -> index 2 (plane 1).
void makeHalfTile(uint8_t* tile, uint8_t loRow, uint8_t hiRow) {
    for (int y = 0; y < 8; ++y) {
        tile[2 * y] = loRow;
        tile[2 * y + 1] = hiRow;
    }
}

gbbg::BgState basicState() {
    gbbg::BgState s{};
    std::memset(s.vram, 0, sizeof(s.vram));
    // Tile 1 at $8010.
    makeHalfTile(&s.vram[16], 0xF0, 0x0F);
    s.lcdc = 0x91;  // LCD on, $8000 tiles, $9800 map, BG on
    s.bgp = 0xE4;
    return s;
}

}  // namespace

TEST(bgmap, base_selection) {
    EXPECT_EQ(gbbg::bgMapBase(0x00), 0x1800);
    EXPECT_EQ(gbbg::bgMapBase(0x08), 0x1C00);
}

TEST(bgmap, entry_wraps_modulo_32) {
    gbbg::BgState s = basicState();
    s.vram[0x1800 + 0] = 1;              // (0,0)
    s.vram[0x1800 + 31 * 32 + 31] = 1;   // bottom-right corner
    EXPECT_EQ(gbbg::mapEntry(s, 0, 0), 1);
    EXPECT_EQ(gbbg::mapEntry(s, 31, 31), 1);
    // Scrolled past the edge wraps back to the same entries.
    EXPECT_EQ(gbbg::mapEntry(s, 32, -32), 1);
    EXPECT_EQ(gbbg::mapEntry(s, -1, 63), 1);
}

TEST(bgdata, signed_and_unsigned_modes) {
    const uint8_t lcdcU = 0x10, lcdcS = 0x00;
    EXPECT_EQ(gbbg::tileDataOffset(lcdcU, 0x01), 0x0010);
    // Signed mode: index byte is relative to $1000 (tile 0x80 == tile 0).
    EXPECT_EQ(gbbg::tileDataOffset(lcdcS, 0x01), 0x1010);   // +1
    EXPECT_EQ(gbbg::tileDataOffset(lcdcS, 0x7F), 0x17F0);   // +127
    EXPECT_EQ(gbbg::tileDataOffset(lcdcS, 0x80), 0x0800);   // -128
    EXPECT_EQ(gbbg::tileDataOffset(lcdcS, 0xFF), 0x0FF0);   // -1
}

TEST(bgline, unscrolled_line_uses_map_tiles) {
    gbbg::BgState s = basicState();
    // Fill row 0 of the map with tile 1.
    for (int i = 0; i < 32; ++i) s.vram[0x1800 + i] = 1;
    uint8_t sh[160];
    gbbg::renderBgScanline(s, 0, sh);
    EXPECT_EQ(sh[0], 1);    // left half of the tile
    EXPECT_EQ(sh[3], 1);
    EXPECT_EQ(sh[4], 2);    // right half
    EXPECT_EQ(sh[159], 2);  // 20th tile also right half
}

TEST(bgline, scroll_wraps_pixels) {
    gbbg::BgState s = basicState();
    for (int i = 0; i < 32 * 32; ++i) s.vram[0x1800 + i] = 1;
    s.scx = 4;  // shifts surface left by 4 px: screen x0 shows surface x4
    uint8_t sh[160];
    gbbg::renderBgScanline(s, 0, sh);
    EXPECT_EQ(sh[0], 2);   // surface pixel x=4 -> right half of tile 1
    EXPECT_EQ(sh[156], 1); // wrapped into the next tile's left half
    s.scy = 200;             // deep scroll: surface y wraps past 256
    gbbg::renderBgScanline(s, 100, sh);  // surface y = (300 & 255) = 44
    EXPECT_EQ(sh[0], 2);
}

TEST(bgline, disabled_bg_is_shade_zero) {
    gbbg::BgState s = basicState();
    for (int i = 0; i < 32; ++i) s.vram[0x1800 + i] = 1;
    s.lcdc &= ~gbbg::kLcdcBgEnable;
    uint8_t sh[160];
    gbbg::renderBgScanline(s, 40, sh);
    bool allZero = true;
    for (int x = 0; x < 160; ++x) allZero = allZero && sh[x] == 0;
    EXPECT_TRUE(allZero);
}
