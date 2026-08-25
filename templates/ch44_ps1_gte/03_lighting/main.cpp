#define LABSTEST_MAIN
#include "labstest.hpp"
#include "lighting.hpp"

using namespace gte;

namespace {
void set_light_matrix(Cop2& g, int16_t l11, int16_t l12, int16_t l13,
                      int16_t l21, int16_t l22, int16_t l23, int16_t l31,
                      int16_t l32, int16_t l33) {
    const int16_t m[9] = {l11, l12, l13, l21, l22, l23, l31, l32, l33};
    for (unsigned i = 0; i < 9; ++i) {
        const uint32_t w = g.cr(8u + i / 2u);
        const uint32_t mask = i % 2 == 0 ? 0x0000FFFFu : 0xFFFF0000u;
        g.wc(8u + i / 2u,
             (w & ~mask) |
                 (static_cast<uint32_t>(static_cast<uint16_t>(m[i]))
                  << (i % 2 == 0 ? 0 : 16)));
    }
}
}  // namespace

TEST(lighting, light_row_dot_wide_math) {
    Cop2 g;
    set_light_matrix(g, 4096, 0, 0, 0, 2048, 0, 0, 0, -1024);
    const auto d0 = light_row_dot(g, 0, {4096, 0, 0}, 12);
    EXPECT_EQ(d0, 4096);                       // 1.0 * 1.0
    const auto d1 = light_row_dot(g, 1, {4096, 4096, 0}, 12);
    EXPECT_EQ(d1, 2048);                       // 0.5 * 1.0 on y
    const auto d2 = light_row_dot(g, 2, {4096, 0, 4096}, 12);
    EXPECT_EQ(d2, -1024);                      // -0.25 * 1.0
}

TEST(lighting, ncds_full_pipeline_hand_computed) {
    Cop2 g;
    // Light matrix: row0 = (1,0,0), rows 1/2 zero.
    set_light_matrix(g, 4096, 0, 0, 0, 0, 0, 0, 0, 0);
    // Light-color matrix: row0 = LR = (4096,0,0) => R channel only.
    for (unsigned w = 15; w <= 20; ++w) g.wc(w, 0);
    g.wc(15, 4096);                            // LR1 = 1.0
    // Background color black; far color black; material pure red.
    g.wc(13, 0); g.wc(14, 0);
    g.wc(21, 0); g.wc(22, 0); g.wc(23, 0);
    g.wd(6, 0x000000C8u);                      // R=200 G=0 B=0 code=0
    // Normal V0 = (1,0,0): IR1 after step1 = 4096.
    g.wd(0, 4096);

    const NcdsResult r = ncds(g, /*lm=*/false);
    EXPECT_EQ(r.ir1, 4096);
    // Step2: MAC = LR1*IR1 = 4096*... wait LCM row0=(4096,0,0)*IR -> IR1=4096? no:
    // lcm_lane(0)=4096, sum = 4096*4096 >>12 = 4096 -> IR1 stays 4096.
    EXPECT_EQ(r.ir1, 4096);
    // Step3: t = RBK*4096 + R*IR1 = 0 + 200*4096 = 819200; >>12 = 200.
    EXPECT_EQ(r.mac0, 819200);
    EXPECT_EQ(r.ir0, 200);
    // Step4: R = (FC_R*4096 + IR0*R) >> 12 = 200*200 >> 12 = 9.
    EXPECT_EQ(r.r, 9);
    EXPECT_EQ(r.g, 0);
    EXPECT_EQ(r.b, 0);
}

TEST(mvmva, rotation_times_ir_with_translation) {
    Cop2 g;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            g.set_rot(i, j, i == j ? 4096 : 0);
    g.set_tr(0, 512);                          // adds 512 after SF shift
    g.set_tr(1, 0); g.set_tr(2, 0);
    g.wd(9, 1000);                             // IR1
    g.wd(10, 0); g.wd(11, 0);

    const Command c = decode_command((1u << 10) | (0u << 16) | (1u << 14) |
                                     0x12u);   // SF=1, mat=rot, vec=IR
    EXPECT_TRUE(c.add12);
    mvmva(g, c);
    // SF=1: products shifted right by 12 => 1.0*IR1 + TRX.
    EXPECT_EQ(g.cr(24), 1000 + 512);
    EXPECT_EQ(g.cr(25), 0u);
}

TEST(mvmva, vector_select_two_uses_sz3) {
    Cop2 g;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            g.set_rot(i, j, 0);
    g.set_rot(2, 2, 2048);                     // row2 z-coeff 0.5
    g.wd(9, 0); g.wd(10, 0);                   // IR1=IR2=0
    g.wd(19, 4096);                            // SZ3

    const Command c = decode_command((1u << 10) | (0u << 16) | (2u << 14) |
                                     0x12u);   // SF=1, vec=(IR1,IR2,SZ3)
    mvmva(g, c);
    EXPECT_EQ(g.cr(24), 0u);
    EXPECT_EQ(g.cr(26), 2048);                 // 0.5 * SZ3 after SF shift
}

TEST(avsz3, averages_three_depths_with_zsf3) {
    Cop2 g;
    g.wc(29, 0x2AAAA);                         // ZSF3 ~= 2.6667 (1.19.12)
    g.wd(17, 1000); g.wd(18, 2000); g.wd(19, 3000);
    avsz3(g);
    // prod = 0x2AAAA * 6000 = 4194195000? exact: 174326*6000 = 1045956000
    // mac0 = prod>>2 = 261489000; ir0 = mac0>>12 = 63839 > 32767 -> clamp
    const int64_t prod = int64_t(0x2AAAA) * 6000;
    EXPECT_EQ(static_cast<int32_t>(g.rd(24)), static_cast<int32_t>(prod >> 2));
    EXPECT_EQ(static_cast<int16_t>(g.rd(8)), 32767);
    EXPECT_NE(g.flag() & kFlagIrSatSigned, 0u);
}
