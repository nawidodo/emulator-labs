#define LABSTEST_MAIN
#include "labstest.hpp"
#include "idct.hpp"

#include <cstdlib>

using namespace mdec;

TEST(idct, zero_block_stays_zero) {
    int in[64] = {}, out[64] = {};
    idct8x8(in, out);
    for (unsigned i = 0; i < 64; ++i) EXPECT_EQ(out[i], 0);
}

TEST(idct, dc_only_produces_flat_output) {
    // Pure DC: every spatial sample must be (nearly) identical — the
    // basis is orthonormal so rounding differences stay within +-1.
    int in[64] = {}, out[64] = {};
    in[0] = 4096;
    idct8x8(in, out);
    const int ref = out[0];
    EXPECT_TRUE(ref > 0);  // 1/4 * c0^2 * scale^-2 * 4096 > 0
    for (unsigned i = 0; i < 64; ++i)
        EXPECT_TRUE(std::abs(out[i] - ref) <= 1);
}

TEST(idct, transpose_symmetry_within_rounding) {
    // The separable transform satisfies idct(F^T) ~= idct(F)^T. The two
    // passes round at different stages (>>5 floor vs >>12 nearest), so
    // the transpose may differ by AT MOST one LSB per sample — a strong
    // structural invariant of a correctly wired basis.
    int in[64], out_a[64], out_b[64];
    for (unsigned i = 0; i < 64; ++i)
        in[i] = static_cast<int>((i * 37) % 97) - 48;
    idct8x8(in, out_a);
    int tin[64];
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) tin[x * 8 + y] = in[y * 8 + x];
    idct8x8(tin, out_b);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            EXPECT_TRUE(std::abs(out_b[y * 8 + x] - out_a[x * 8 + y]) <= 1);
}

TEST(idct, low_frequency_energy_dominates_center) {
    // A single low horizontal frequency produces a row-varying pattern.
    int in[64] = {}, out[64] = {};
    in[1] = 1024;  // F[y][u=1]: varies along x with period 8
    idct8x8(in, out);
    // Row 0 samples must NOT be flat (frequency present).
    bool varies = false;
    for (int x = 1; x < 8; ++x)
        if (out[x] != out[0]) varies = true;
    EXPECT_TRUE(varies);
}
