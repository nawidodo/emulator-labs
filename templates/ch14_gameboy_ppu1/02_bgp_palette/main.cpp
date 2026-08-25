#define LABSTEST_MAIN
#include <cstdio>

#include "labstest.hpp"
#include "palette.hpp"

TEST(bgp, field_selection) {
    // Default power-up palette 0b11100100: idx0->0, idx1->1, idx2->2,
    // idx3->3 (identity).
    const uint8_t def = 0xE4;
    EXPECT_EQ(gbpal::applyBGP(0, def), 0);
    EXPECT_EQ(gbpal::applyBGP(1, def), 1);
    EXPECT_EQ(gbpal::applyBGP(2, def), 2);
    EXPECT_EQ(gbpal::applyBGP(3, def), 3);

    // Inverted palette 0b00011011 flips the order — this is how games
    // flash backgrounds between two color schemes.
    const uint8_t inv = 0x1B;
    EXPECT_EQ(gbpal::applyBGP(0, inv), 3);
    EXPECT_EQ(gbpal::applyBGP(1, inv), 2);
    EXPECT_EQ(gbpal::applyBGP(2, inv), 1);
    EXPECT_EQ(gbpal::applyBGP(3, inv), 0);
}

TEST(bgp, grayscale_ramp) {
    const gbpal::Rgba white{255, 255, 255, 255};
    const gbpal::Rgba black{0, 0, 0, 255};
    gbpal::Rgba c = gbpal::shadeToRgba(0);
    EXPECT_EQ(c.r, white.r);
    EXPECT_EQ(c.a, 255);
    c = gbpal::shadeToRgba(3);
    EXPECT_EQ(c.r, black.r);
    EXPECT_EQ(c.g, black.g);
    EXPECT_EQ(c.b, black.b);
    // Mid shades are distinct so frames stay unambiguous under hashing.
    EXPECT_NE(gbpal::shadeToRgba(1).r, gbpal::shadeToRgba(2).r);
}
