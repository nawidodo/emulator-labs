#define LABSTEST_MAIN
#include "labstest.hpp"
#include "dp.hpp"

using namespace arm;

// Instruction encoders (kept local so tests read like assembly).
static constexpr uint32_t dp_imm(DpOp op, bool s, uint32_t rn, uint32_t rd,
                                 uint32_t rot, uint32_t imm8) {
    return 0xE0000000u | (1u << 25) | (op << 21) | (s ? 1u << 20 : 0) |
           (rn << 16) | (rd << 12) | (rot << 8) | imm8;
}
static constexpr uint32_t dp_reg(DpOp op, bool s, uint32_t rn, uint32_t rd,
                                 uint32_t sh_type, uint32_t sh_amt,
                                 uint32_t rm) {
    return 0xE0000000u | (op << 21) | (s ? 1u << 20 : 0) | (rn << 16) |
           (rd << 12) | (sh_amt << 7) | (sh_type << 5) | rm;
}

TEST(dp, immediate_rotate_operand) {
    DpCpu cpu;
    // MOV r0, #0xFF000000 == ror(0xFF, 8): rot=4.
    const uint32_t instr = dp_imm(kMOV, false, 0, 0, 4, 0xFF);
    EXPECT_EQ(cpu.read_operand2(instr), 0xFF000000u);
    EXPECT_TRUE(cpu.shifter_carry_valid);   // rot != 0: carry = bit31 of result
    EXPECT_TRUE(cpu.shifter_carry);
    // rot == 0 leaves the shifter carry invalid (C unchanged in hardware).
    cpu.read_operand2(dp_imm(kMOV, false, 0, 0, 0, 0x42));
    EXPECT_FALSE(cpu.shifter_carry_valid);
}

TEST(dp, register_shifted_operand) {
    DpCpu cpu;
    cpu.r[1] = 1;
    // Rm=r1, LSL #4 -> 0x10, carry from old bit28 (=0).
    EXPECT_EQ(cpu.read_operand2(dp_reg(kADD, false, 0, 2, kLSL, 4, 1)), 0x10u);
    EXPECT_TRUE(cpu.shifter_carry_valid);
    EXPECT_FALSE(cpu.shifter_carry);
}

TEST(dp, add_with_carry_edges) {
    auto out = DpCpu::add_with_carry(0xFFFFFFFFu, 1, false);
    EXPECT_EQ(out.value, 0u);
    EXPECT_TRUE(out.carry);
    EXPECT_FALSE(out.overflow);
    // Signed overflow: 0x7FFFFFFF + 1.
    out = DpCpu::add_with_carry(0x7FFFFFFFu, 1, false);
    EXPECT_EQ(out.value, 0x80000000u);
    EXPECT_TRUE(out.overflow);
}

TEST(dp, subs_carry_is_inverted_borrow) {
    DpCpu cpu;
    cpu.r[1] = 5;
    cpu.exec_dp(dp_imm(kSUB, true, 1, 0, 0, 10));  // 5-10 borrows
    EXPECT_EQ(cpu.r[0], 0xFFFFFFFBu);
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);            // borrow cleared C
    EXPECT_NE((cpu.cpsr & FLAG_N), 0u);
    cpu.r[2] = 20;
    cpu.exec_dp(dp_imm(kSUB, true, 2, 0, 0, 5));   // 20-5: no borrow
    EXPECT_EQ(cpu.r[0], 15u);
    EXPECT_NE((cpu.cpsr & FLAG_C), 0u);            // C = NOT borrow
}

TEST(dp, adc_chain_64bit) {
    DpCpu cpu;
    // ADDS r2, r1(0xFFFFFFFF), #2 -> r2=1, carry out.
    cpu.r[1] = 0xFFFFFFFFu;
    cpu.exec_dp(dp_imm(kADD, true, 1, 2, 0, 2));
    EXPECT_EQ(cpu.r[2], 1u);
    EXPECT_NE((cpu.cpsr & FLAG_C), 0u);
    // ADCS r3, r3(0), #0 picks the carry up into the high word.
    cpu.exec_dp(dp_imm(kADC, true, 3, 3, 0, 0));
    EXPECT_EQ(cpu.r[3], 1u);
}

TEST(dp, plain_add_ignores_carry_flag) {
    DpCpu cpu;
    // A borrow leaves C=0; a following plain ADD must NOT consume it as a
    // carry-in (that is ADC's job). 5 + 3 == 8 regardless of C.
    cpu.r[1] = 5;
    cpu.exec_dp(dp_imm(kSUB, true, 1, 0, 0, 10));  // borrows: C=0
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);
    cpu.exec_dp(dp_imm(kADD, false, 1, 4, 0, 3));  // r4 = r1(5) + 3
    EXPECT_EQ(cpu.r[4], 8u);
}

TEST(dp, sbc_uses_carry_not_borrow) {
    DpCpu cpu;
    cpu.r[1] = 0;
    cpu.r[2] = 1;
    // SUBS r0,r1,#1 -> 0-1 clears C.
    cpu.exec_dp(dp_imm(kSUB, true, 1, 0, 0, 1));
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);
    // SBCS r3,r1,r2 = 0 - 1 - !C(1) = -2.
    cpu.exec_dp(dp_reg(kSBC, true, 1, 3, kLSL, 0, 2));
    EXPECT_EQ(cpu.r[3], 0xFFFFFFFEu);
}

TEST(dp, tst_cmp_do_not_writeback) {
    DpCpu cpu;
    cpu.r[1] = 0xF0F0;
    cpu.r[2] = 0x0F0F;
    cpu.exec_dp(dp_reg(kTST, true, 1, 7, kLSL, 0, 2));  // would write r7
    EXPECT_EQ(cpu.r[7], 0u);
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);                 // 0xF0F0 & 0x0F0F = 0
    cpu.r[3] = 0x0F0F;
    cpu.exec_dp(dp_reg(kCMP, true, 2, 9, kLSL, 0, 3));  // would write r9
    EXPECT_EQ(cpu.r[9], 0u);
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);                 // equal operands
}

TEST(dp, logical_s_uses_shifter_carry_not_alu_carry) {
    // THE curriculum point. A preceding SUBS leaves C reflecting a borrow;
    // MOVS with a plain immediate must leave C alone, while MOVS with RRX
    // must source its C from the *shifter*.
    DpCpu cpu;
    cpu.r[1] = 5;
    cpu.r[2] = 10;
    cpu.exec_dp(dp_imm(kSUB, true, 1, 0, 0, 10));       // borrow -> C=0
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);
    // MOV r0, #0x42 with rot==0: C must stay 0 even though result nonzero.
    cpu.exec_dp(dp_imm(kMOV, true, 0, 0, 0, 0x42));
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);
    // MOVS r0, r1, RRX (encoded as ROR imm %0): pulls C=0 into bit31,
    // pushes old bit0 of r1 (=1) into C.
    cpu.r[1] = 0x00000003u;
    cpu.exec_dp(dp_reg(kMOV, true, 0, 0, kROR, 0, 1));
    EXPECT_EQ(cpu.r[0], 0x00000001u);
    EXPECT_NE((cpu.cpsr & FLAG_C), 0u);
}

TEST(dp, movs_preserves_v_flag) {
    DpCpu cpu;
    cpu.set_nzcv(false, false, true, true);
    cpu.exec_dp(dp_imm(kMOV, true, 0, 0, 0, 1));
    EXPECT_NE((cpu.cpsr & FLAG_V), 0u);                 // V survives logical ops
}

TEST(hidden, dp_hidden_semantics) {
    DpCpu cpu;
    // RSB: 10 - r1(3) = 7, no borrow -> C set.
    cpu.r[1] = 3;
    cpu.exec_dp(dp_imm(kRSB, true, 1, 4, 0, 10));
    EXPECT_EQ(cpu.r[4], 7u);
    EXPECT_NE((cpu.cpsr & FLAG_C), 0u);
    // BIC with shifted operand: r2 &~ (r1<<1).
    cpu.r[1] = 1;
    cpu.r[2] = 0b110;
    cpu.exec_dp(dp_reg(kBIC, false, 2, 5, kLSL, 1, 1));
    EXPECT_EQ(cpu.r[5], 0b100u);
    // MVN #0: all ones, negative, nonzero.
    cpu.exec_dp(dp_imm(kMVN, true, 0, 6, 0, 0));
    EXPECT_EQ(cpu.r[6], 0xFFFFFFFFu);
    EXPECT_NE((cpu.cpsr & FLAG_N), 0u);
}
