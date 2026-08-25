#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_gte.hpp"

using gte::Cop2;
using gtedbg::Result;

namespace {
void setup(Cop2& g) {
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            g.set_rot(i, j, i == j ? 4096 : 0);
    g.set_h(512); g.set_ofx(0); g.set_ofy(0);
}
}  // namespace

TEST(debug_gte, lm_clamps_negative_ir_to_zero) {
    Cop2 g;
    setup(g);
    g.wd(0, static_cast<uint32_t>(static_cast<uint16_t>(-30000)) |
                (0u << 16));
    g.wd(1, 4000);
    const Result r = gtedbg::project(g, /*lm=*/true);
    EXPECT_EQ(r.ir1, 0);  // LM=1: no negative IR allowed
    EXPECT_NE(r.flag & gte::kFlagIrSatUnsigned, 0u);
}

TEST(debug_gte, signed_mode_keeps_negative_ir) {
    Cop2 g;
    setup(g);
    g.wd(0, static_cast<uint32_t>(static_cast<uint16_t>(-30000)) |
                (0u << 16));
    g.wd(1, 4000);
    const Result r = gtedbg::project(g, /*lm=*/false);
    EXPECT_EQ(r.ir1, -30000);
}

TEST(debug_gte, divide_overflow_raises_flag_and_forces_max) {
    Cop2 g;
    setup(g);
    g.wd(0, 0);                       // V=(0,?,?) -> depth MAC3 = TRZ = 0
    g.wd(1, 0);
    const Result r = gtedbg::project(g, false);
    EXPECT_NE(r.flag & gte::kFlagDivideOvf, 0u);
    EXPECT_NE(r.flag & 0x80000000u, 0u);          // ERROR aggregate
    EXPECT_EQ(r.mac0, 0x1FFFF);
    EXPECT_EQ(r.ir0, 0x1FFFF);
    EXPECT_EQ(r.sx2, 1023);
}
