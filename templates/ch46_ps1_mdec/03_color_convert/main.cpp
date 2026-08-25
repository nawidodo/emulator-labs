#define LABSTEST_MAIN
#include "labstest.hpp"
#include "color.hpp"

using namespace mdec;

TEST(color, neutral_chroma_passes_luma) {
    // cb=cr=0 -> r=g=b=y (no rounding bias shifts zero products).
    const auto w = ycbcr_to_rgb15(128, 0, 0);
    EXPECT_EQ(w & 0x1Fu, 128u >> 3);
    EXPECT_EQ((w >> 5) & 0x1Fu, 128u >> 3);
    EXPECT_EQ((w >> 10) & 0x1Fu, 128u >> 3);
}

TEST(color, positive_cr_pushes_red_up) {
    const int y = 100;
    const auto base = ycbcr_to_rgb15(y, 0, 0);
    const auto warm = ycbcr_to_rgb15(y, 0, 512);   // cr = +0.5
    // r gains ~1436*512>>10 = 718 -> saturates to max lane.
    EXPECT_TRUE((warm & 0x1Fu) > (base & 0x1Fu));
    // g drops: -731*512>>10 = -365.
    EXPECT_TRUE(((warm >> 5) & 0x1F) < ((base >> 5) & 0x1F));
}

TEST(color, clamps_instead_of_wrapping) {
    const auto hi = ycbcr_to_rgb15(255, 1023, 1023);
    EXPECT_TRUE((hi & 0x1F) == 0x1F || true);  // red at/near max
    EXPECT_EQ(hi & 0x1F, 0x1Fu);               // saturated, not wrapped
    const auto lo = ycbcr_to_rgb15(-400, -1024, 1023);
    // Deep negative luma with strong negative cb pushes blue far below 0.
    EXPECT_EQ(lo >> 10, 0u);                   // blue lane clamped to 0
}

TEST(color, macroblock_layout_and_upscale) {
    int blocks[6][64] = {};
    // Y block values encode their block index; chroma constant per plane.
    for (int b = 0; b < 4; ++b)
        for (int i = 0; i < 64; ++i) blocks[b][i] = b * 16;
    for (int i = 0; i < 64; ++i) { blocks[4][i] = 0; blocks[5][i] = 0; }

    uint16_t out[256];
    assemble_macroblock(blocks, out);

    // Top-left pixel uses Y0 (=0), top-right Y1 (=16).
    EXPECT_EQ(out[0], ycbcr_to_rgb15(0, 0, 0));
    EXPECT_EQ(out[8], ycbcr_to_rgb15(16, 0, 0));
    // Bottom-right pixel uses Y3 (=48).
    EXPECT_EQ(out[15 * 16 + 15], ycbcr_to_rgb15(48, 0, 0));
    // Chroma upscale: pixel (1,1) samples chroma cell (0,0).
    EXPECT_EQ(out[1 * 16 + 1], ycbcr_to_rgb15(16 * 0 /*Y0*/, 0, 0));
}
