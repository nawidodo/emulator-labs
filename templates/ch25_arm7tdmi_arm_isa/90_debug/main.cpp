#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_cpu.hpp"

using namespace arm;

static constexpr uint32_t dp_imm(DpOp op, bool s, uint32_t rn, uint32_t rd,
                                 uint32_t rot, uint32_t imm8) {
    return 0xE0000000u | (1u << 25) | (op << 21) | (s ? 1u << 20 : 0) |
           (rn << 16) | (rd << 12) | (rot << 8) | imm8;
}
static constexpr uint32_t branch(bool link, int32_t byte_offset) {
    return 0xE0000000u | (0xA << 24) | (link ? 1u << 24 : 0) |
           ((byte_offset >> 2) & 0xFFFFFF);
}
static constexpr uint32_t dp_reg(DpOp op, bool s, uint32_t rn, uint32_t rd,
                                 uint32_t sh_type, uint32_t sh_amt,
                                 uint32_t rm) {
    return 0xE0000000u | (op << 21) | (s ? 1u << 20 : 0) | (rn << 16) |
           (rd << 12) | (sh_amt << 7) | (sh_type << 5) | rm;
}
// SBCS r3, r1, r2 (LSL #0)
static constexpr uint32_t dp_reg_sbc() { return dp_reg(kSBC, true, 1, 3, arm::kLSL, 0, 2); }
// One test per seeded defect; each names the symptom from DEBUGGING.md.
TEST(debug_suite, bug1_rotate_zero_immediate_must_not_touch_carry) {
    DebugCpu cpu;
    cpu.r[1] = 5;
    cpu.r[2] = 10;
    cpu.exec(dp_imm(kSUB, true, 1, 0, 0, 10), 0);   // borrow -> C=0
    cpu.exec(dp_imm(kMOV, true, 0, 0, 0, 0x42), 0); // rot==0: C must stay 0
    EXPECT_EQ((cpu.cpsr & FLAG_C), 0u);
}

TEST(debug_suite, bug2_adc_carries_the_c_flag) {
    DebugCpu cpu;
    cpu.r[1] = 0xFFFFFFFFu;
    cpu.exec(dp_imm(kADD, true, 1, 2, 0, 2), 0);    // r2=1, C=1
    cpu.exec(dp_imm(kADC, true, 3, 3, 0, 0), 0);    // r3 must become 1
    EXPECT_EQ(cpu.r[3], 1u);
}

TEST(debug_suite, bug3_sbc_borrow_polarity) {
    DebugCpu cpu;
    cpu.r[1] = 0;
    cpu.r[2] = 1;
    cpu.exec(dp_imm(kSUB, true, 1, 0, 0, 1), 0);    // C=0 (borrow)
    cpu.exec(dp_reg_sbc(), 0);
    EXPECT_EQ(cpu.r[3], 0xFFFFFFFEu);            // 0 - 1 - !C(=1)
}

TEST(debug_suite, bug4_cmp_does_not_writeback) {
    DebugCpu cpu;
    cpu.r[2] = 7;
    cpu.exec(dp_imm(kCMP, true, 2, 9, 0, 7), 0);    // equal: Z set, no write
    EXPECT_EQ(cpu.r[9], 0u);
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);
}

TEST(debug_suite, bug5_bl_return_address_is_pc_plus_4) {
    DebugCpu cpu;
    // BL at 0x00 to 0x08; LR must be 0x04.
    cpu.write32(0x00, branch(true, 0));
    cpu.write32(0x08, branch(false, -8));        // callee returns via B back
    cpu.step();
    EXPECT_EQ(cpu.r[14], 0x04u);
    EXPECT_EQ(cpu.r[15], 0x08u);
}
