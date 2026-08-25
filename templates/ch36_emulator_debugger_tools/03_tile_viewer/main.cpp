#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "tiles.hpp"

TEST(tiles, decode_row_shades) {
    // Shades: 0=' ', 1='.', 2='+', 3='#'.
    EXPECT_EQ(view::decode_row(0xFF, 0x00), "........");
    EXPECT_EQ(view::decode_row(0x00, 0xFF), "++++++++");
    // Left nibble from low plane only (value 1), right from high only (2).
    EXPECT_EQ(view::decode_row(0xF0, 0x0F), "....++++");
    // Both planes on = shade 3.
    EXPECT_EQ(view::decode_row(0xFF, 0xFF), "########");
}

TEST(tiles, render_tile_layout) {
    // Tile 1 at vram+16: rows alternate full '#' then blank.
    uint8_t vram[32] = {};
    for (int r = 0; r < 8; ++r) {
        vram[16 + r * 2] = r % 2 ? 0x00 : 0xFF;
        vram[16 + r * 2 + 1] = r % 2 ? 0x00 : 0xFF;
    }
    const auto lines = view::render_tile(vram, 1);
    EXPECT_EQ(lines.size(), size_t{8});
    EXPECT_EQ(lines[0], "########");
    EXPECT_EQ(lines[1], "        ");
    EXPECT_EQ(lines[7], "        ");
}

TEST(chip8fb, renders_scanlines) {
    uint8_t fb[64 * 32] = {};
    fb[0] = 1;                 // top-left pixel
    fb[31 * 64 + 63] = 1;      // bottom-right pixel
    const auto lines = view::render_chip8_fb(fb);
    EXPECT_EQ(lines.size(), size_t{32});
    EXPECT_EQ(lines[0][0], '#');
    EXPECT_EQ(lines[0][1], '.');
    EXPECT_EQ(lines[31][63], '#');
    EXPECT_EQ(lines[15].find('#'), std::string::npos);
}
