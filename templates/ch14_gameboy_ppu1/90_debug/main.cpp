// Tests for 90_debug: each test targets exactly one seeded defect. All
// three must be fixed before the suite goes green.
#define LABSTEST_MAIN
#include <cstring>

#include "labstest.hpp"
#include "renderer.hpp"

TEST(debug_bgp, identity_palette_maps_in_order) {
    const uint8_t def = 0xE4;
    EXPECT_EQ(gbdbg::applyBGP(0, def), 0);
    EXPECT_EQ(gbdbg::applyBGP(1, def), 1);
    EXPECT_EQ(gbdbg::applyBGP(2, def), 2);
    EXPECT_EQ(gbdbg::applyBGP(3, def), 3);
}

TEST(debug_bgp, inverted_palette_reverses_shades) {
    const uint8_t inv = 0x1B;
    EXPECT_EQ(gbdbg::applyBGP(0, inv), 3);
    EXPECT_EQ(gbdbg::applyBGP(3, inv), 0);
}

TEST(debug_map, coordinates_wrap_modulo_32) {
    gbdbg::PpuState s{};
    std::memset(&s, 0, sizeof(s));
    s.vram[0x1800 + 0] = 7;              // (0,0)
    s.vram[0x1800 + 31 * 32 + 31] = 9;   // (31,31)
    EXPECT_EQ(gbdbg::mapEntry(s, 0, 0), 7);
    EXPECT_EQ(gbdbg::mapEntry(s, 31, 31), 9);
    // Scrolled past the edge wraps back onto the map.
    EXPECT_EQ(gbdbg::mapEntry(s, 32, 0), 7);
    EXPECT_EQ(gbdbg::mapEntry(s, -1, -1), 9);
    EXPECT_EQ(gbdbg::mapEntry(s, 64, 32), 7);
}

TEST(debug_tiles, signed_mode_addresses_relative_to_1000) {
    const uint8_t lcdcU = 0x10, lcdcS = 0x00;
    EXPECT_EQ(gbdbg::tileDataOffset(lcdcU, 0x01), 0x0010);
    EXPECT_EQ(gbdbg::tileDataOffset(lcdcS, 0x01), 0x1010);  // +1
    EXPECT_EQ(gbdbg::tileDataOffset(lcdcS, 0x80), 0x0800);  // -128
    EXPECT_EQ(gbdbg::tileDataOffset(lcdcS, 0xFF), 0x0FF0);  // -1
    EXPECT_EQ(gbdbg::tileDataOffset(lcdcS, 0x7F), 0x17F0);  // +127
}
