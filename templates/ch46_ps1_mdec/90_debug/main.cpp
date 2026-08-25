#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_mdec.hpp"

#include <vector>

using namespace mdbg;
using namespace mdec;

TEST(debug_mdec, coefficients_land_in_natural_order) {
    // DC at natural 0 plus run=2 AC at natural 8 (zig-zag position 2).
    std::vector<uint16_t> units = {
        0x0010u,                                            // scale 16
        0x0002u,                                            // DC level +2
        static_cast<uint16_t>((2u << 10) | 0x3FDu),         // run2 lvl -3
        kEndOfBlock,
    };
    int out[64];
    decode_to_natural(units.data(), units.size(), out);
    EXPECT_EQ(out[0], 64);     // DC: 2*16*16/8
    EXPECT_EQ(out[8], -48);    // zig-zag pos 2 unscanned to natural 8
    EXPECT_EQ(out[1], 0);
    EXPECT_EQ(out[2], 0);
}

TEST(debug_mdec, samples_saturate_not_wrap) {
    EXPECT_EQ(sample_byte(300), 255);
    EXPECT_EQ(sample_byte(-40), 0);
    EXPECT_EQ(sample_byte(128), 128);
}
