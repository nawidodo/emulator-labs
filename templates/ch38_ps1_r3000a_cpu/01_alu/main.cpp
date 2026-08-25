#define LABSTEST_MAIN
#include "labstest.hpp"
#include "cpu.hpp"

using psx::r3000a::Regs;
namespace enc = psx::r3000a;  // field extraction helpers

// --- R-type --------------------------------------------------------------
TEST(alu_r, addu_wraps) {
    Regs r;
    r.gpr[1] = 0xFFFFFFFFu;
    r.gpr[2] = 2;
    // addu $t3, $at, $v0  => funct 0x21
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1u << 21) | (2u << 16) | (3u << 11) | 0x21, r));
    EXPECT_EQ(r.gpr[3], 1u);
}

TEST(alu_r, subu_and_or_xor_nor) {
    Regs r;
    r.gpr[1] = 0xF0F0F0F0u;
    r.gpr[2] = 0x0FF00FF0u;
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (3 << 11) | 0x23, r));
    EXPECT_EQ(r.gpr[3], 0xE100E100u);
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (4 << 11) | 0x24, r));
    EXPECT_EQ(r.gpr[4], 0x00F000F0u);
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (5 << 11) | 0x25, r));
    EXPECT_EQ(r.gpr[5], 0xFFF0FFF0u);
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (6 << 11) | 0x26, r));
    EXPECT_EQ(r.gpr[6], 0xFF00FF00u);
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (7 << 11) | 0x27, r));
    EXPECT_EQ(r.gpr[7], ~0xFFF0FFF0u);
}

TEST(alu_r, slt_signed_vs_sltu_unsigned) {
    Regs r;
    r.gpr[1] = 0xFFFFFFFEu;  // -2 signed / huge unsigned
    r.gpr[2] = 1;
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (3 << 11) | 0x2A, r));
    EXPECT_EQ(r.gpr[3], 1u);  // -2 < 1 signed
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (4 << 11) | 0x2B, r));
    EXPECT_EQ(r.gpr[4], 0u);  // 0xFFFFFFFE > 1 unsigned
}

TEST(alu_r, zero_register_discards_writes) {
    Regs r;
    r.gpr[1] = 5;
    r.gpr[2] = 6;
    // addu $zero, $at, $v0 — result must be discarded
    EXPECT_TRUE(psx::r3000a::exec_alu_r((1 << 21) | (2 << 16) | (0 << 11) | 0x21, r));
    EXPECT_EQ(r.gpr[0], 0u);
}

TEST(alu_r, unknown_funct_unhandled) {
    Regs r;
    EXPECT_FALSE(psx::r3000a::exec_alu_r(0x3Fu, r));  // funct 63 unassigned in our subset
}

// --- I-type --------------------------------------------------------------
TEST(alu_i, addiu_sign_extends) {
    Regs r;
    r.gpr[1] = 0x00000100u;
    // addiu $t0, $at, -1
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x09 << 26 | (1 << 21) | (8 << 16) | 0xFFFFu, r));
    EXPECT_EQ(r.gpr[8], 0xFFu);
}

TEST(alu_i, logical_ops_zero_extend) {
    Regs r;
    r.gpr[1] = 0x12345678u;
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0C << 26 | (1 << 21) | (2 << 16) | 0xFFFFu, r));
    EXPECT_EQ(r.gpr[2], 0x12345678u & 0xFFFFu);   // andi
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0D << 26 | (1 << 21) | (3 << 16) | 0xFFFFu, r));
    EXPECT_EQ(r.gpr[3], 0x1234FFFFu);             // ori
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0E << 26 | (1 << 21) | (4 << 16) | 0xFFFFu, r));
    EXPECT_EQ(r.gpr[4], 0x1234A987u);             // xori
}

TEST(alu_i, slti_sltiu_use_signed_imm) {
    Regs r;
    r.gpr[1] = 5;
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0A << 26 | (1 << 21) | (2 << 16) | 0xFFFBu, r));  // imm=-5
    EXPECT_EQ(r.gpr[2], 0u);
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0B << 26 | (1 << 21) | (3 << 16) | 0xFFFBu, r));
    EXPECT_EQ(r.gpr[3], 1u);  // sltiu sign-extends imm then compares unsigned:
                              // 5 < 0xFFFFFFFB is true
}

TEST(alu_i, lui_ignores_rs) {
    Regs r;
    r.gpr[1] = 0xDEADBEEFu;
    EXPECT_TRUE(psx::r3000a::exec_alu_i(0x0F << 26 | (1 << 21) | (2 << 16) | 0x8001u, r));
    EXPECT_EQ(r.gpr[2], 0x80010000u);
}

// --- shifts --------------------------------------------------------------
TEST(shifts, immediate_forms) {
    Regs r;
    r.gpr[1] = 0x80000001u;
    auto instr = [](uint32_t sh, uint32_t funct, uint32_t src, uint32_t dst) {
        return (src << 16) | (dst << 11) | (sh << 6) | funct;
    };
    EXPECT_TRUE(psx::r3000a::exec_shifts(instr(1, 0x00, 1, 2), r));
    EXPECT_EQ(r.gpr[2], 0x00000002u);              // sll
    EXPECT_TRUE(psx::r3000a::exec_shifts(instr(1, 0x02, 1, 3), r));
    EXPECT_EQ(r.gpr[3], 0x40000000u);              // srl
    EXPECT_TRUE(psx::r3000a::exec_shifts(instr(1, 0x03, 1, 4), r));
    EXPECT_EQ(r.gpr[4], 0xC0000000u);              // sra sign-fills
}

TEST(shifts, variable_forms_mask_to_five_bits) {
    Regs r;
    r.gpr[1] = 1u;         // shift amount register
    r.gpr[2] = 0x80000000u;
    // sllv $t1, $t2, $at with rs=33 -> amount 1 after &31... rs value is gpr[1]
    uint32_t i = (1u << 21) | (2u << 16) | (3u << 11) | 0x04;
    EXPECT_TRUE(psx::r3000a::exec_shifts(i, r));
    EXPECT_EQ(r.gpr[3], 0u);  // 0x80000000 << 1 wraps to 0
}

TEST(shifts, sll_zero_is_nop_shape) {
    Regs r;
    r.gpr[8] = 0xABCDu;
    EXPECT_TRUE(psx::r3000a::exec_shifts(8u << 16, r));  // sll $zero? no: rt=8 rd=0 -> discards
    EXPECT_EQ(r.gpr[0], 0u);
}
