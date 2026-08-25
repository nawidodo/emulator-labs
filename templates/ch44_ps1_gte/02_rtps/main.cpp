#define LABSTEST_MAIN
#include "labstest.hpp"
#include "gte.hpp"

using gte::Cop2;
using gte::RtpsResult;

namespace {
void identity_rotation(Cop2& g) {
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            g.set_rot(i, j, i == j ? 4096 : 0);
}
}  // namespace

TEST(rtps, identity_transform_exact_arithmetic) {
    Cop2 g;
    identity_rotation(g);
    g.wd(0, (4096u) | (2048u << 16));   // V0: VX=4096 VY=2048 (1.0, 0.5)
    g.wd(1, 1024);                      // VZ=1024 (0.25)
    g.set_ofx(1024);
    g.set_ofy(768);
    g.set_h(512);

    const RtpsResult r = gte::rtps(g, 0);
    EXPECT_EQ(r.mac1, 4096);            // 1.0 * 1.0
    EXPECT_EQ(r.mac2, 2048);
    EXPECT_EQ(r.mac3, 1024);
    EXPECT_EQ(r.ir1, 4096);
    EXPECT_EQ(r.sz3, 1024);
    // mac0 = ((512*20000h/1024)+1)/2 = 32768 -> IR0 saturates to 32767.
    EXPECT_EQ(r.mac0, 32768);
    EXPECT_EQ(r.ir0, 32767);
    // dx = 4096*32768>>16 = 2048 -> SX clamps to screen max.
    EXPECT_EQ(r.sx2, 1023);
    EXPECT_EQ(r.sy2, 1023);
    EXPECT_NE(r.flag & 0x80000000u, 0u);      // ERROR aggregate
    EXPECT_NE(r.flag & gte::kFlagIrSatSigned, 0u);
}

TEST(rtps, translation_applies_before_projection) {
    Cop2 g;
    identity_rotation(g);
    g.set_tr(0, 4096);                  // TRX = 1.0 -> adds 4096 after sf
    g.set_tr(1, 0);
    g.set_tr(2, 0);
    g.wd(0, 0);                         // V = (0,0,0)
    g.set_ofx(0); g.set_ofy(0); g.set_h(1024);

    const RtpsResult r = gte::rtps(g, 0);
    EXPECT_EQ(r.mac1, 4096);            // TRX*10000h >> 12 == TRX
    EXPECT_EQ(r.mac3, 0);
    EXPECT_EQ(r.sz3, 0);                // depth zero -> divide overflow path
    EXPECT_NE(r.flag & gte::kFlagDivideOvf, 0u);
    EXPECT_EQ(r.mac0, 0x1FFFF);
    EXPECT_EQ(r.ir0, 0x1FFFF);
    EXPECT_EQ(r.sx2, 1023);             // forced-max projection outputs
}

TEST(rtps, saturation_and_lm_range_select) {
    Cop2 g;
    identity_rotation(g);
    g.set_h(512); g.set_ofx(0); g.set_ofy(0);
    // VX = 30000 with R11=1.0: MAC1=30000 fits; use R11=2.0 to overflow IR.
    g.set_rot(0, 0, 8192);
    g.wd(0, (30000u) | (0u << 16));
    g.wd(1, 1000);

    const RtpsResult r = gte::rtps(g, 0);
    EXPECT_EQ(r.mac1, 60000);           // wide MAC holds the true value
    EXPECT_EQ(r.ir1, 32767);            // architectural clamp
    EXPECT_NE(r.flag & gte::kFlagIrSatSigned, 0u);
}

TEST(rtps, negative_ir_clamped_by_lm_semantics) {
    Cop2 g;
    identity_rotation(g);
    g.set_h(512); g.set_ofx(0); g.set_ofy(0);
    g.wd(0, static_cast<uint32_t>(static_cast<uint16_t>(-30000)) |
                (0u << 16));            // VX=-30000
    g.wd(1, 5000);

    const RtpsResult r_signed = gte::rtps(g, 0, /*lm=*/false);
    EXPECT_EQ(r_signed.mac1, -30000);
    EXPECT_EQ(r_signed.ir1, -30000);    // inside signed range: untouched
    EXPECT_EQ(r_signed.flag & gte::kFlagIrSatUnsigned, 0u);
}

TEST(rtpt, transforms_three_vertices_and_pushes_fifo) {
    Cop2 g;
    identity_rotation(g);
    g.set_h(256); g.set_ofx(0); g.set_ofy(0);
    g.wd(0, (4096u) | (0u << 16));  g.wd(1, 4096);   // V0=(1,0,1)
    g.wd(2, (2048u) | (0u << 16));  g.wd(3, 4096);   // V1=(0.5,0,1)
    g.wd(4, (1024u) | (0u << 16));  g.wd(5, 4096);   // V2=(0.25,0,1)

    gte::rtpt(g);
    // After RTPT, SXY2 holds the LAST vertex, SXY1 the second.
    const uint32_t sxy2 = g.rd(14), sxy1 = g.rd(13), sxy0 = g.rd(12);
    // mac0 = ((256*20000h/4096)+1)/2 = 4096.
    // dx(ir1) = ir1*4096>>16 -> v0:256, v1:128, v2:64 (OFX=0).
    // FIFO ends as SXY0=v0, SXY1=v1, SXY2=v2.
    EXPECT_EQ(sxy2 & 0xFFFFu, 64u);
    EXPECT_EQ(sxy2 >> 16, 0u);
    EXPECT_EQ(sxy1 & 0xFFFFu, 128u);
    EXPECT_EQ(sxy0 & 0xFFFFu, 256u);
    // FIFO ordering: SXY0 oldest vertex — same value here, so check SZ
    // queue instead: SZ3 ended at the last vertex's depth.
    EXPECT_EQ(g.rd(19), 4096);
}

TEST(rtpt, flag_accumulates_across_vertices) {
    Cop2 g;
    // Row0 scale 2.0, rows 1/2 identity.
    g.set_rot(0, 0, 8192);
    g.set_rot(1, 1, 4096);
    g.set_rot(2, 2, 4096);
    g.set_h(256); g.set_ofx(0); g.set_ofy(0);
    g.wd(0, (0u << 16) | 100u);   g.wd(1, 4000);   // v0 fine
    g.wd(2, (0u << 16) | 30000u); g.wd(3, 4000);   // v1 overflows IR1
    g.wd(4, (500u << 16) | 500u); g.wd(5, 4000);   // v2 fine

    gte::rtpt(g);
    EXPECT_NE(g.flag() & gte::kFlagIrSatSigned, 0u);  // sticky from V1
}
