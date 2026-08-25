#define LABSTEST_MAIN
#include "labstest.hpp"
#include "regressions.hpp"

using namespace psxmini::regress;

// One test per seeded bug; the suite names carry the seed number so hidden
// manifest cases can pin each regression individually:
//   ch50_90_regress_tests seed01   ...   ch50_90_regress_tests seed10

TEST(seed01_alu, addi_sign_extends_imm12) {
    // 0xFFF is -1 in 12-bit two's complement: 5 + (-1) == 4.
    EXPECT_EQ(exec_addi(5, 0xFFF), 4u);
    // 0x7FE stays positive: 0 + 2046 == 2046.
    EXPECT_EQ(exec_addi(0, 0x7FE), 2046u);
    // Countdown reaching zero: 1 + (-1) == 0, not 1 + 4095.
    EXPECT_EQ(exec_addi(1, 0xFFF), 0u);
}

TEST(seed02_trace, line_reports_entry_pc) {
    StepCpu c;
    c.pc = 0x00000100;
    c.cyc = 41;
    const std::string line = step_trace(c, 0x09100000);
    EXPECT_EQ(line, std::string("pc=00000100 op=09100000 cyc=42"));
    EXPECT_EQ(c.pc, 0x104u);  // pc still advances for the NEXT fetch
}

TEST(seed03_gpu_fill, covers_requested_width) {
    RVram v;
    gpu_fill(v, 10, 5, 4, 2, 0x7FFF);
    unsigned lit = 0;
    for (unsigned j = 5; j < 7; ++j)
        for (unsigned i = 10; i < 14; ++i)
            if (v.p[j * RVram::kW + i] == 0x7FFF) ++lit;
    EXPECT_EQ(lit, 8u);  // 4 x 2 pixels, right edge INCLUDED
    EXPECT_EQ(v.p[5 * RVram::kW + 13], static_cast<uint16_t>(0x7FFF));
    EXPECT_EQ(v.p[5 * RVram::kW + 14], static_cast<uint16_t>(0));
}

TEST(seed04_vram_blit, honours_source_stride) {
    // Source buffer: 3 rows of a 4-pixel texture stored with stride 8
    // (four padding words between rows), values row*16+i.
    uint16_t tex[24] = {};
    for (unsigned j = 0; j < 3; ++j)
        for (unsigned i = 0; i < 4; ++i)
            tex[j * 8 + i] = static_cast<uint16_t>(j * 16 + i);

    RVram v;
    blit_rows(v, tex, /*src_stride=*/8, /*x=*/4, /*y=*/2, /*rows=*/3,
              /*cols=*/4);
    // First pixel of each landed row identifies its source row.
    EXPECT_EQ(v.p[2 * RVram::kW + 4], static_cast<uint16_t>(0));
    EXPECT_EQ(v.p[3 * RVram::kW + 4], static_cast<uint16_t>(16));
    EXPECT_EQ(v.p[4 * RVram::kW + 4], static_cast<uint16_t>(32));
    // The last column of each row arrives intact...
    EXPECT_EQ(v.p[3 * RVram::kW + 7], static_cast<uint16_t>(19));
    // ...and row padding must NOT leak into VRAM.
    EXPECT_EQ(v.p[2 * RVram::kW + 8], static_cast<uint16_t>(0));
    EXPECT_EQ(v.p[3 * RVram::kW + 9], static_cast<uint16_t>(0));
}

TEST(seed05_env, exponential_release_quantum_once) {
    // Level 29184 at rate code 7: delta = (29184*7)>>6 = 3192, so one
    // release step lands on 25992 — a double subtraction would give 22800.
    EXPECT_EQ(env_exp_release(29184, 7), 25992);
    // Small levels fall by at least 1 per sample and clamp at silence
    // instead of wrapping negative.
    EXPECT_EQ(env_exp_release(1, 15), 0);
    EXPECT_EQ(env_exp_release(0, 15), 0);
}

TEST(seed06_dma, transfers_exactly_wc_words) {
    uint32_t ram[64];
    for (unsigned i = 0; i < 64; ++i) ram[i] = 0x1000u + i;
    RDma ch;
    ch.madr = 0x40;          // word index 16
    ch.words_left = 5;

    uint32_t dev[8];
    for (uint32_t& d : dev) d = 0xDEADBEEFu;  // sentinel

    const uint32_t copied = dma_run_block(ch, ram, 64, dev);
    EXPECT_EQ(copied, 5u);
    EXPECT_EQ(dev[0], 0x1010u);
    EXPECT_EQ(dev[4], 0x1014u);
    EXPECT_EQ(dev[5], 0xDEADBEEFu);  // nothing stomped behind the buffer
    EXPECT_EQ(ch.madr, 0x40u + 4 * 5);
    EXPECT_EQ(ch.words_left, 0u);
    EXPECT_FALSE(ch.enable);
    EXPECT_TRUE(ch.irq);
}

TEST(seed07_dma_chain, clears_enable_when_chain_retires) {
    uint32_t ram[64];
    for (unsigned i = 0; i < 64; ++i) ram[i] = 0x2000u + i;
    const RDesc chain[2] = {
        {0x00, 2, 1},     // two words from word 0, continue at descriptor 1
        {0x20, 3, 0xFFFF} // three words from word 8, end of chain
    };
    RDma ch;
    ch.enable = true;
    uint32_t dev[8];

    dma_run_chain(ch, chain, 2, ram, 64, dev, 8);

    EXPECT_EQ(dev[0], 0x2000u);
    EXPECT_EQ(dev[1], 0x2001u);
    EXPECT_EQ(dev[2], 0x2008u);
    EXPECT_EQ(dev[4], 0x200Au);
    EXPECT_EQ(ch.words_left, 0u);
    EXPECT_FALSE(ch.enable);  // the poll loop must observe a retired channel
    EXPECT_TRUE(ch.irq);
}

TEST(seed08_gte, shifts_once_after_the_dot_product) {
    // m0*vx = 3000, m1*vy = 3000: the sum 6000 crosses one >>12 unit while
    // neither term does — shifting per-term loses it.
    EXPECT_EQ(gte_mac_y(3000, 3000, 1, 1, 0), 1);
    // And translation applies after the shift either way:
    // dot = 4096*2 = 8192, >>12 = 2, + ty 5 == 7.
    EXPECT_EQ(gte_mac_y(4096, 0, 2, 0, 5), 7);
}

TEST(seed09_timer, reloads_on_exact_target_match) {
    RTimer t;  // target 37
    for (unsigned i = 0; i < 38; ++i) timer_tick(t);
    // The 38th effective tick hits cnt==target and reloads to zero.
    EXPECT_EQ(t.reached, 1u);
    EXPECT_EQ(t.cnt, 0u);
    for (unsigned i = 0; i < 38; ++i) timer_tick(t);
    EXPECT_EQ(t.reached, 2u);  // periodic, no drift
    EXPECT_EQ(t.cnt, 0u);
}

TEST(seed10_cdrom, decodes_bcd_msf_fields) {
    EXPECT_EQ(cdrom_bcd_to_dec(0x59), 59u);
    EXPECT_EQ(cdrom_bcd_to_dec(0x10), 10u);
    EXPECT_EQ(cdrom_bcd_to_dec(0x99), 99u);
    EXPECT_EQ(cdrom_bcd_to_dec(0x04), 4u);
}
