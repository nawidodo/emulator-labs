#define LABSTEST_MAIN
#include "labstest.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include "dbg_render.hpp"

using namespace nes21dbg;

namespace {

// One attribute byte, quadrants painted 0/1/2/3 (value 0xE4).
constexpr uint8_t kQuadrants = 0xE4;

}  // namespace

// Regression test: every coarse position inside a 4x4-tile block must map to
// the quadrant the hardware spec assigns. RED while the defect is present.
TEST(nes21dbg, quadrant_map_matches_hardware) {
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            int expect = ((y & 2) ? 2 : 0) | ((x & 2) ? 1 : 0);
            EXPECT_EQ(attribute_bits(kQuadrants, x, y), expect);
        }
    }
}

TEST(nes21dbg, quadrant_shift_only_depends_on_bit1) {
    EXPECT_EQ(attribute_bits(0b00'00'00'01 & 0xFF, 0, 0), 1);   // TL = bits0-1
    EXPECT_EQ(attribute_bits(0b00'00'01'00 & 0xFF, 3, 1), 1);   // TR = bits2-3
    EXPECT_EQ(attribute_bits(0b00'01'00'00 & 0xFF, 1, 3), 1);   // BL = bits4-5
    EXPECT_EQ(attribute_bits(0b01'00'00'00 & 0xFF, 3, 3), 1);   // BR = bits6-7
}

TEST(nes21dbg, rendered_tile_colors_come_from_correct_quadrant) {
    std::array<uint8_t, 0x2000> chr{};
    std::array<uint8_t, 0x20> pal{};
    // Solid tile: both planes full -> tile color 3 everywhere.
    for (int fy = 0; fy < 16; ++fy) chr[fy] = 0xFF;
    // Palette selects: entry 3 of palette p has a distinct value.
    pal[0x03] = 0x10;
    pal[0x07] = 0x20;
    pal[0x0B] = 0x30;
    pal[0x0F] = 0x40;

    int attr = attribute_bits(kQuadrants, 2, 2);  // bottom-right quadrant
    std::vector<int> seen;
    auto collect = [&](int, int, int pal_idx) { seen.push_back(pal_idx); };
    render_tile(chr.data(), 0x0000, 0, attr, 16, 16, collect, pal.data());

    EXPECT_EQ(seen.size(), size_t(64));
    for (int v : seen) EXPECT_EQ(v, 0x40);  // BR palette's color 3 ($3F0F)
}
