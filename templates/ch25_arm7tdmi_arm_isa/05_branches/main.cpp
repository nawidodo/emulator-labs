#define LABSTEST_MAIN
#include "labstest.hpp"
#include "arm_cpu.hpp"

using namespace arm;

static constexpr uint32_t dp_imm(DpOp op, bool s, uint32_t rn, uint32_t rd,
                                 uint32_t rot, uint32_t imm8) {
    return 0xE0000000u | (1u << 25) | (op << 21) | (s ? 1u << 20 : 0) |
           (rn << 16) | (rd << 12) | (rot << 8) | imm8;
}
static constexpr uint32_t branch(bool link, int32_t byte_offset) {
    const uint32_t imm = (byte_offset >> 2) & 0x00FFFFFF;
    return 0xE0000000u | (0xA << 24) | (link ? 1u << 24 : 0) | imm;
}
static constexpr uint32_t bx(uint32_t rm) { return 0xE12FFF10u | rm; }

TEST(branches, offset_sign_extension) {
    ArmCpu cpu;
    EXPECT_EQ(cpu.branch_offset(0xEAFFFFFE), -8);   // backward branch
    EXPECT_EQ(cpu.branch_offset(0xEA000000), 0);
    EXPECT_EQ(cpu.branch_offset(0xEA000001), 4);
    // Negative field: 0x800000 -> -8 MiB of instructions -> *4 bytes.
    EXPECT_EQ(cpu.branch_offset(0xEA800000), -(32 << 20));
}

TEST(branches, b_target_uses_pc_plus_8) {
    ArmCpu cpu;
    // Branch at pc=0 skipping one instruction: target = 0 + 8 + (-8) = 0.
    uint32_t next = 0xDEAD;
    const unsigned cyc = cpu.exec_branch(branch(false, -8), 0x0000, next);
    EXPECT_EQ(next, 0x0000u);
    EXPECT_EQ(cyc, 3u);                             // taken: 2S + 1N
}

TEST(branches, bl_saves_pc_minus_4) {
    ArmCpu cpu;
    // BL at 0x1000: LR must be the instruction after the branch (pc+4),
    // NOT the pipeline's pc+8 read value.
    uint32_t next = 0;
    cpu.exec_branch(branch(true, 0x40), 0x1000, next);  // +0x40 bytes
    EXPECT_EQ(cpu.r[14], 0x1004u);
    EXPECT_EQ(next, 0x1000u + 8 + 0x40);
}

TEST(branches, bx_sets_thumb_bit) {
    ArmCpu cpu;
    cpu.r[2] = 0x08000101;                          // odd address: Thumb
    uint32_t next = 0;
    cpu.exec_bx(bx(2), next);
    EXPECT_TRUE(cpu.thumb);
    EXPECT_EQ(next, 0x08000100u);
    cpu.r[3] = 0x03000000;                          // even: stays ARM
    cpu.exec_bx(bx(3), next);
    EXPECT_FALSE(cpu.thumb);
    EXPECT_EQ(next, 0x03000000u);
}

TEST(branches, skipped_instruction_still_advances) {
    ArmCpu cpu;
    // cond=NE with Z clear executes; cond=EQ would be skipped but PC moves.
    cpu.cpsr = 0;                                   // Z clear
    cpu.write32(0x100, dp_imm(kMOV, false, 0, 5, 0, 7));   // MOVNE r5,#7
    cpu.write32(0x104, 0x0A000000);                 // BEQ +0 (never taken)
    cpu.r[15] = 0x100;
    cpu.step();
    EXPECT_EQ(cpu.r[5], 7u);                        // NE executed
    EXPECT_EQ(cpu.r[15], 0x104u);
    cpu.r[15] = 0x104;
    const unsigned cyc = cpu.step();                // EQ skipped: 1S
    EXPECT_EQ(cyc, 1u);
    EXPECT_EQ(cpu.r[15], 0x108u);
}

TEST(branches, call_return_roundtrip) {
    ArmCpu cpu;
    // 0x00: BL 0x08 ; 0x04: B self(halt) ; 0x08: MOV r0,#42 ; 0x0C: BX lr
    cpu.write32(0x00, branch(true, 0));
    cpu.write32(0x04, branch(false, 0));
    cpu.write32(0x08, dp_imm(kMOV, false, 0, 0, 0, 42));
    cpu.write32(0x0C, bx(14));
    cpu.step();                                     // BL
    EXPECT_EQ(cpu.r[15], 0x08u);
    EXPECT_EQ(cpu.r[14], 0x04u);
    cpu.step();                                     // MOV
    cpu.step();                                     // BX lr
    EXPECT_EQ(cpu.r[15], 0x04u);
    EXPECT_FALSE(cpu.thumb);
}

TEST(hidden, branches_hidden_semantics) {
    ArmCpu cpu;
    // 0x00: MOV r0,#3 ; 0x04: SUBS r0,r0,#1 ; 0x08: BNE 0x04 ; 0x0C: B .
    cpu.write32(0x00, dp_imm(kMOV, false, 0, 0, 0, 3));
    cpu.write32(0x04, dp_imm(kSUB, true, 0, 0, 0, 1));
    cpu.write32(0x08, 0x10000000u | (0xA << 24) | ((-12 >> 2) & 0xFFFFFF));
    cpu.write32(0x0C, branch(false, -8));              // b .
    for (int i = 0; i < 20; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 0u);
    EXPECT_EQ(cpu.r[15], 0x0Cu);
}
