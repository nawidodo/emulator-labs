#define LABSTEST_MAIN
#include "labstest.hpp"

#include "addressing.hpp"

TEST(addressing, dp_basic_and_indexed) {
    snescpu::CpuState c;
    c.d = 0x0200;
    // No index: dp $10 -> $0210 in bank 0.
    EXPECT_EQ(snescpu::dp_effective(c, 0x10, 0), 0x0210);
    // Index inside the same page.
    EXPECT_EQ(snescpu::dp_effective(c, 0xF0, 0x05), 0x02F5);
    // Index wraps the direct page window within bank zero.
    EXPECT_EQ(snescpu::dp_effective(c, 0xFF, 0x20), 0x031F);
}

TEST(addressing, dp_page_cross_penalty) {
    snescpu::CpuState c;
    c.d = 0x0200;
    EXPECT_FALSE(snescpu::dp_page_crossed(c, 0x10, 0));
    EXPECT_TRUE(snescpu::dp_page_crossed(c, 0xFF, 0x01));
    // The penalty comes from the INDEX addition crossing a page, so an
    // unindexed access never pays it regardless of D alignment.
    c.d = 0x12FF;
    EXPECT_FALSE(snescpu::dp_page_crossed(c, 0x01, 0));
}

TEST(addressing, dp_index_width_matters) {
    snescpu::CpuState c;
    c.d = 0x0000;
    // A wide index can cross a page on its own; an 8-bit index cannot.
    EXPECT_TRUE(snescpu::dp_page_crossed(c, 0x00, 0x0104));
}

TEST(addressing, abs_uses_db_bank) {
    snescpu::CpuState c;
    c.db = 0x7E;
    EXPECT_EQ(snescpu::abs_effective(c, 0x1234), 0x7E1234u);
}

TEST(addressing, abs_indexed_wraps_in_bank) {
    snescpu::CpuState c;
    c.db = 0x3F;
    bool crossed = false;
    const uint32_t ea =
        snescpu::abs_indexed(c, 0xFFFF, 0x0002, &crossed);
    EXPECT_EQ(ea, 0x3F0001u);   // wrapped, did NOT spill into next bank
    EXPECT_TRUE(crossed);
    crossed = false;
    snescpu::abs_indexed(c, 0x2000, 0x0010, &crossed);
    EXPECT_FALSE(crossed);
}

TEST(addressing, long_ignores_db) {
    snescpu::CpuState c;
    c.db = 0xAA;
    EXPECT_EQ(snescpu::long_effective(0x018000u), 0x018000u);
}

TEST(addressing, stack_relative_in_bank_zero) {
    snescpu::CpuState c;
    c.sp = 0x01F0;
    EXPECT_EQ(snescpu::sr_effective(c, 0x10), 0x0200);
}
