#define LABSTEST_MAIN
#include "labstest.hpp"
#include <bit>

#include "flags.hpp"

using namespace i8080;

TEST(parity, even_counts_set_flag) {
    EXPECT_TRUE(even_parity(0x00));   // 0 bits -> even
    EXPECT_TRUE(even_parity(0x03));   // 2 bits
    EXPECT_TRUE(even_parity(0xFF));   // 8 bits
    EXPECT_FALSE(even_parity(0x01));  // 1 bit
    EXPECT_FALSE(even_parity(0x80));
    EXPECT_FALSE(even_parity(0xFE));
}

TEST(parity, all_byte_values) {
    unsigned even = 0, odd = 0;
    for (int v = 0; v < 256; ++v) {
        int bits = std::popcount(unsigned(v));
        if (even_parity(uint8_t(v)))
            EXPECT_EQ(bits % 2, 0);
        else
            EXPECT_EQ(bits % 2, 1);
        (bits % 2 == 0 ? even : odd)++;
    }
    // Half of the byte space is even parity.
    EXPECT_EQ(even, 128u);
    EXPECT_EQ(odd, 128u);
}

TEST(szp, combines_sign_zero_parity) {
    EXPECT_EQ(szp_bits(0x00), FLAG_Z | FLAG_P);
    EXPECT_EQ(szp_bits(0x80), FLAG_S);
    EXPECT_EQ(szp_bits(0x01), 0);              // odd parity, positive
    EXPECT_EQ(szp_bits(0x03), FLAG_P);         // two bits = even parity
    EXPECT_EQ(szp_bits(0x07), 0);              // three bits = odd parity
    EXPECT_EQ(szp_bits(0xFF), FLAG_S | FLAG_P);
}

TEST(szp, never_leaks_other_bits) {
    for (int v = 0; v < 256; ++v) {
        uint8_t f = szp_bits(uint8_t(v));
        EXPECT_EQ(f & (FLAG_AC | FLAG_CY | 0x02 | 0x08 | 0x20), 0);
    }
}

TEST(aux_carry, add_half_sum) {
    EXPECT_FALSE(aux_carry_add(0x01, 0x02, false));
    EXPECT_TRUE(aux_carry_add(0x0F, 0x01, false));   // nibble overflow
    EXPECT_TRUE(aux_carry_add(0x01, 0x0F, true));    // carry pushes it over
    EXPECT_FALSE(aux_carry_add(0x10, 0x20, false));  // bit 4 carries, not bit 3
    EXPECT_TRUE(aux_carry_add(0xFF, 0xFF, true));
    EXPECT_FALSE(aux_carry_add(0xF0, 0x0F, false));  // full-byte carry only
}

// The hardware computes subtraction as a + ~b + !borrow, so its AC is the
// half-CARRY of that sum: set whenever low(a) >= low(b) (for borrow_in=0).
// Equal low nibbles therefore SET AC even though nothing "borrows" — a real
// 8080/Z80 silicon quirk diagnostics rely on.
TEST(aux_carry, sub_complement_form) {
    EXPECT_FALSE(aux_carry_sub(0x10, 0x01, false));  // true nibble borrow
    EXPECT_TRUE(aux_carry_sub(0x10, 0x00, false));   // equal-low quirk!
    EXPECT_TRUE(aux_carry_sub(0x0F, 0x0F, false));   // equal-low quirk
    EXPECT_FALSE(aux_carry_sub(0x00, 0x01, false));
    EXPECT_FALSE(aux_carry_sub(0x00, 0x00, true));   // borrow_in suppresses
    EXPECT_TRUE(aux_carry_sub(0x20, 0x10, false));   // high-nibble only ops
}

TEST(aux_carry, daa_critical_edges) {
    // ADD 0x09 + 0x01: no aux carry (9+1 stays inside the nibble);
    // DAA fixes the digit through its >9 rule instead of AC.
    EXPECT_FALSE(aux_carry_add(0x09, 0x01, false));
    // ADD 0x99 + 0x01 = 0x9A raw: nibble sum 9+1 stays inside 0-F,
    // so AC is CLEAR and DAA must fix the digit via its >9 rule.
    EXPECT_FALSE(aux_carry_add(0x99, 0x01, false));
    // Contrast: 0x29 + 0x09 really overflows the nibble -> AC set.
    EXPECT_TRUE(aux_carry_add(0x29, 0x09, false));
}

TEST(psw_pack, bit1_always_set) {
    EXPECT_EQ(pack_psw(0x00), 0x02);
    EXPECT_EQ(pack_psw(0xFF & ~uint8_t(0x02)), 0xFF);
}
