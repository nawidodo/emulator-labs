#define LABSTEST_MAIN
#include "labstest.hpp"
#include "shifter.hpp"

using arm::ShiftResult;

TEST(shifter, lsl_exact) {
    EXPECT_EQ(arm::shift_lsl(4, 0x80000001u, false).value, 0x00000010u);
    EXPECT_EQ(arm::shift_lsl(4, 0x10000000u, false).carry_out, true);   // bit28
    EXPECT_EQ(arm::shift_lsl(32, 1, false).value, 0u);
    EXPECT_EQ(arm::shift_lsl(32, 1, false).carry_out, true);            // old bit0
    EXPECT_EQ(arm::shift_lsl(33, 0xFFFFFFFFu, true).carry_out, false);  // >32: 0
    // amount 0: value passes, C unchanged.
    const auto l0 = arm::shift_lsl(0, 0x12345678u, true);
    EXPECT_EQ(l0.value, 0x12345678u);
    EXPECT_TRUE(l0.carry_out);
}

TEST(shifter, lsr_exact) {
    EXPECT_EQ(arm::shift_lsr(4, 0x10, false).value, 1u);
    EXPECT_EQ(arm::shift_lsr(4, 0x18, false).carry_out, true);          // bit3
    EXPECT_EQ(arm::shift_lsr(32, 0x80000000u, false).carry_out, true);
    EXPECT_EQ(arm::shift_lsr(40, 0xFFFFFFFFu, false).value, 0u);
    EXPECT_FALSE(arm::shift_lsr(40, 0xFFFFFFFFu, false).carry_out);
}

TEST(shifter, asr_exact) {
    const uint32_t neg = 0xFFFFFF00u;
    EXPECT_EQ(arm::shift_asr(4, neg, false).value, 0xFFFFFFF0u);
    EXPECT_EQ(arm::shift_asr(16, neg, false).carry_out, true);     // bit15 of src
    EXPECT_EQ(arm::shift_asr(32, 0x80000000u, false).value, 0xFFFFFFFFu);
    EXPECT_TRUE(arm::shift_asr(32, 0x40000000u, false).value == 0);
    EXPECT_FALSE(arm::shift_asr(32, 0x40000000u, false).carry_out);
    EXPECT_EQ(arm::shift_asr(255, 0xC0000000u, false).value, 0xFFFFFFFFu);
}

TEST(shifter, ror_and_rrx) {
    EXPECT_EQ(arm::shift_ror(4, 0x000000F1u, false).value, 0x1000000Fu);
    EXPECT_FALSE(arm::shift_ror(4, 0x000000F1u, false).carry_out);  // old bit3 = 0
    // ROR by exactly 32 keeps the value, carry = bit31.
    const auto r32 = arm::shift_ror(32, 0x80000003u, false);
    EXPECT_EQ(r32.value, 0x80000003u);
    EXPECT_TRUE(r32.carry_out);
    // RRX pulls old C into bit31 and pushes bit0 out.
    const auto x1 = arm::shift_rrx(2, true);
    EXPECT_EQ(x1.value, 0x80000001u);
    EXPECT_FALSE(x1.carry_out);
    const auto x2 = arm::shift_rrx(2, false);
    EXPECT_EQ(x2.value, 0x00000001u);
    EXPECT_FALSE(x2.carry_out);
    const auto x3 = arm::shift_rrx(3, false);
    EXPECT_EQ(x3.value, 0x00000001u);
    EXPECT_TRUE(x3.carry_out);
}

TEST(shifter, imm_dispatch_specials) {
    // LSR imm5==0 means shift by 32.
    const auto l = arm::shift_imm(arm::kLSR, 0, 0x80000000u, true);
    EXPECT_EQ(l.value, 0u);
    EXPECT_TRUE(l.carry_out);
    // ROR imm5==0 means RRX.
    const auto ri = arm::shift_imm(arm::kROR, 0, 2, true);
    const auto rr = arm::shift_rrx(2, true);
    EXPECT_EQ(ri.value, rr.value);
    EXPECT_EQ(ri.carry_out, rr.carry_out);
}

TEST(shifter, reg_dispatch) {
    // Only low byte of Rs counts.
    EXPECT_EQ(arm::shift_reg(arm::kLSL, 0x104, 1, false).value, 0x10u);
    // Register-ROR with count 0 rotates a full 32 places: carry = bit31.
    EXPECT_EQ(arm::shift_reg(arm::kROR, 0, 0x80000000u, false).carry_out, true);
    EXPECT_EQ(arm::shift_reg(arm::kROR, 0, 0x12345678u, false).value,
              0x12345678u);
}

namespace {

// Independent reference model written from the ARM ARM description so the
// sweep does not just mirror the implementation's structure.
ShiftResult ref(uint32_t type, uint32_t amt, uint32_t rm, bool cin) {
    if (type == arm::kLSL) {
        if (amt == 0) return {rm, cin};
        if (amt < 32) return {rm << amt, ((rm >> (32 - amt)) & 1) != 0};
        if (amt == 32) return {0, (rm & 1) != 0};
        return {0, false};
    }
    if (type == arm::kLSR) {
        if (amt == 0) return {rm, cin};
        if (amt < 32) return {rm >> amt, ((rm >> (amt - 1)) & 1) != 0};
        if (amt == 32) return {0, (rm >> 31) != 0};
        return {0, false};
    }
    if (type == arm::kASR) {
        if (amt == 0) return {rm, cin};
        if (amt < 32)
            return {static_cast<uint32_t>(static_cast<int32_t>(rm) >> amt),
                    ((rm >> (amt - 1)) & 1) != 0};
        return {rm >> 31 ? 0xFFFFFFFFu : 0u, (rm >> 31) != 0};
    }
    // ROR
    if (amt == 0) return {rm, cin};
    const uint32_t n = amt & 31;
    if (n == 0) return {rm, (rm >> 31) != 0};
    return {(rm >> n) | (rm << (32 - n)), ((rm >> (n - 1)) & 1) != 0};
}

}  // namespace

TEST(shifter, sweep_against_reference) {
    for (uint32_t type = 0; type < 4; ++type)
        for (uint32_t amt = 0; amt <= 255; ++amt) {
            const uint32_t rm = 0xDEADBEEFu ^ (amt * 0x01010193u);
            const auto got = arm::shift_reg(type, amt, rm, true);
            const auto want = ref(type, amt, rm, true);
            EXPECT_EQ(got.value, want.value);
            EXPECT_EQ(got.carry_out, want.carry_out);
        }
}

TEST(hidden, shifter_carry_hidden) {
    // Hidden grader suite: edge amounts that expose off-by-one carries,
    // asserted against hand-computed hardware results.
    const auto lsl1 = arm::shift_reg(arm::kLSL, 1, 0x80000000u, false);
    EXPECT_EQ(lsl1.value, 0u);
    EXPECT_TRUE(lsl1.carry_out);                       // old bit31
    EXPECT_FALSE(arm::shift_reg(arm::kLSL, 31, 0x40000001u, false).carry_out);
    EXPECT_TRUE(arm::shift_reg(arm::kLSL, 32, 1, false).carry_out);
    EXPECT_EQ(arm::shift_reg(arm::kLSL, 40, 1, false).value, 0u);
    EXPECT_FALSE(arm::shift_reg(arm::kLSL, 40, 1, false).carry_out);
    EXPECT_TRUE(arm::shift_reg(arm::kLSR, 32, 0x80000000u, false).carry_out);
    EXPECT_EQ(arm::shift_reg(arm::kASR, 33, 0x80000000u, false).value,
              0xFFFFFFFFu);
    EXPECT_EQ(arm::shift_reg(arm::kASR, 33, 0x7FFFFFFFu, false).value, 0u);
    const auto ror1 = arm::shift_reg(arm::kROR, 1, 1, true);
    EXPECT_EQ(ror1.value, 0x80000000u);
    EXPECT_TRUE(ror1.carry_out);                       // old bit0
    EXPECT_EQ(arm::shift_reg(arm::kROR, 16, 0xABCD1234u, false).value,
              0x1234ABCDu);
    EXPECT_FALSE(
        arm::shift_reg(arm::kROR, 16, 0xABCD1234u, false).carry_out);
    EXPECT_TRUE(arm::shift_rrx(1, false).carry_out);
}
