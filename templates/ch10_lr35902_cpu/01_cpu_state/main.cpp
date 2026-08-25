#define LABSTEST_MAIN
#include "labstest.hpp"
#include "cpu.hpp"

TEST(flags, pack_and_read_back) {
    gb::Registers r;
    r.f = 0;
    r.set_z(true);
    r.set_c(true);
    EXPECT_EQ(r.f, gb::FLAG_Z | gb::FLAG_C);
    EXPECT_TRUE(r.flag_z());
    EXPECT_FALSE(r.flag_n());
    EXPECT_FALSE(r.flag_h());
    EXPECT_TRUE(r.flag_c());
}

TEST(flags, low_nibble_reads_zero) {
    gb::Registers r;
    r.set_f(0xFF);
    EXPECT_EQ(r.f, 0xF0);
}

TEST(flags, setters_clear_too) {
    gb::Registers r;
    r.set_f(0xF0);
    r.set_z(true);
    r.set_z(false);
    EXPECT_FALSE(r.flag_z());
    r.set_h(true);
    r.set_h(false);
    EXPECT_FALSE(r.flag_h());
}

TEST(pairs, boot_values_form_pairs) {
    gb::Registers r;
    r.reset();
    EXPECT_EQ(r.af(), 0x01B0);
    EXPECT_EQ(r.bc(), 0x0013);
    EXPECT_EQ(r.de(), 0x00D8);
    EXPECT_EQ(r.hl(), 0x014D);
    EXPECT_EQ(r.sp, 0xFFFE);
    EXPECT_EQ(r.pc, 0x0100);
}
TEST(pairs, set_round_trips) {
    gb::Registers r;
    r.set_bc(0xBEEF);
    EXPECT_EQ(r.b, 0xBE);
    EXPECT_EQ(r.c, 0xEF);
    EXPECT_EQ(r.bc(), 0xBEEF);
    r.set_de(0xCAFE);
    EXPECT_EQ(r.de(), 0xCAFE);
    r.set_hl(0x1234);
    EXPECT_EQ(r.h, 0x12);
    EXPECT_EQ(r.l, 0x34);
}

TEST(pairs, set_af_masks_low_nibble) {
    gb::Registers r;
    r.set_af(0xABCD);
    EXPECT_EQ(r.a, 0xAB);
    EXPECT_EQ(r.af(), 0xABC0);
}
