#define LABSTEST_MAIN
#include "labstest.hpp"

#include "alu.hpp"

using namespace i8080;

TEST(alu_add, basic_and_carry) {
    auto r = alu_add(0x01, 0x02, false);
    EXPECT_EQ(r.value, 0x03);
    EXPECT_FALSE(r.cy);
    EXPECT_FALSE(r.ac);

    r = alu_add(0xFF, 0x01, false);
    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.cy);
    EXPECT_TRUE(r.ac);  // nibble also wrapped
}

TEST(alu_add, carry_in) {
    auto r = alu_add(0x00, 0x00, true);
    EXPECT_EQ(r.value, 0x01);
    EXPECT_FALSE(r.cy);

    r = alu_add(0xFF, 0xFF, true);
    EXPECT_EQ(r.value, 0xFF);
    EXPECT_TRUE(r.cy);
}

TEST(alu_sub, borrow_semantics) {
    auto r = alu_sub(0x05, 0x03, false);
    EXPECT_EQ(r.value, 0x02);
    EXPECT_FALSE(r.cy);  // no borrow

    r = alu_sub(0x03, 0x05, false);
    EXPECT_EQ(r.value, 0xFE);
    EXPECT_TRUE(r.cy);  // borrow taken

    r = alu_sub(0x00, 0x00, true);  // SBB with carry set
    EXPECT_EQ(r.value, 0xFF);
    EXPECT_TRUE(r.cy);
}

TEST(alu_sub, aux_carry_edges) {
    // Complement-add half-carry: AC set iff low(a) >= low(b) (borrow_in=0).
    EXPECT_FALSE(alu_sub(0x10, 0x01, false).ac);
    EXPECT_FALSE(alu_sub(0x10, 0x02, false).ac);
    EXPECT_TRUE(alu_sub(0x10, 0x00, false).ac);   // equal-low quirk
    EXPECT_TRUE(alu_sub(0xF3, 0xB3, false).ac);   // equal-low quirk
}

TEST(alu_ana, clears_carry_quirks_ac) {
    // Bit-3 OR quirk: operands both clear at bit 3 -> AC clear.
    auto r = alu_ana(0xF0, 0xF0);
    EXPECT_EQ(r.value, 0xF0);
    EXPECT_FALSE(r.cy);
    EXPECT_FALSE(r.ac);

    // A=0x08 sets bit 3 in the OR even though the AND result has none.
    r = alu_ana(0x08, 0xF7);
    EXPECT_EQ(r.value, 0x00);
    EXPECT_TRUE(r.ac);

    r = alu_ana(0x0F, 0x37);
    EXPECT_EQ(r.value, 0x07);
    EXPECT_TRUE(r.ac);   // (0x0F|0x37)&0x08 != 0
}

TEST(alu_orx, clear_flags) {
    auto o = alu_ora(0x0F, 0xF0);
    EXPECT_EQ(o.value, 0xFF);
    EXPECT_FALSE(o.cy);
    EXPECT_FALSE(o.ac);

    auto x = alu_xra(0xAA, 0xAA);
    EXPECT_EQ(x.value, 0x00);
    EXPECT_FALSE(x.cy);
    EXPECT_FALSE(x.ac);
}

// CMP is alu_sub with the value discarded — verify the flag contract the
// core relies on.
TEST(alu_cmp, flags_only_via_alu_sub) {
    auto eq = alu_sub(0x42, 0x42, false);
    EXPECT_EQ(eq.value, 0x00);
    EXPECT_FALSE(eq.cy);
    // Zero flag derives from szp_bits(value), applied by the core.

    auto lt = alu_sub(0x41, 0x42, false);
    EXPECT_TRUE(lt.cy);

    auto gt = alu_sub(0x43, 0x42, false);
    EXPECT_FALSE(gt.cy);
}
