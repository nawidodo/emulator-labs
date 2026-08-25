#define LABSTEST_MAIN
#include "labstest.hpp"
#include "thumb_cpu.hpp"

using namespace thumb;

// Thumb encoders (halfwords).
static constexpr uint16_t f1(uint32_t op, uint32_t imm5, uint32_t rm,
                             uint32_t rd) {
    return static_cast<uint16_t>((op << 11) | (imm5 << 6) | (rm << 3) | rd);
}
static constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x2000u | (op << 11) | (rd << 8) | imm8);
}
static constexpr uint16_t f4(uint32_t op, uint32_t rs, uint32_t rd) {
    return static_cast<uint16_t>(0x4000u | (op << 6) | (rs << 3) | rd);
}
static constexpr uint16_t bcond(uint32_t cond, int32_t byte_off) {
    return static_cast<uint16_t>(0xD000u | (cond << 8) |
                                 ((byte_off >> 1) & 0xFF));
}
static constexpr uint16_t bany(int32_t byte_off) {
    return static_cast<uint16_t>(0xE000u | ((byte_off >> 1) & 0x7FF));
}

TEST(pipeline, fetch_advances_pc_by_two) {
    ThumbCpu cpu;
    cpu.write16(0x100, f3(kF3MOV, 0, 7));
    cpu.r[15] = 0x100;
    const uint16_t hw = cpu.fetch();
    EXPECT_EQ(hw, f3(kF3MOV, 0, 7));
    EXPECT_EQ(cpu.r[15], 0x102u);     // instr + 2: the PC an executor sees
}

TEST(pipeline, pc_rel_literal_uses_advanced_pc) {
    ThumbCpu cpu;
    cpu.r[15] = 0x100;
    cpu.write32(0x104, 0xABCD1234u);   // literal at (0x100+4) + 0
    cpu.write16(0x100, static_cast<uint16_t>(0x4800));  // LDR r0,[PC,#0]
    cpu.step();
    EXPECT_EQ(cpu.r[0], 0xABCD1234u);
    // Same instruction two slots later reads a different literal.
    cpu.r[15] = 0x110;
    cpu.write32(0x114, 42);
    cpu.write16(0x110, static_cast<uint16_t>(0x4800));
    cpu.step();
    EXPECT_EQ(cpu.r[0], 42u);
}

TEST(pipeline, imm8_group_flags) {
    ThumbCpu cpu;
    cpu.r[15] = 0;
    // MOV r0, #0xFF then CMP r0, #0xFF (equal: Z set).
    cpu.write16(0, f3(kF3MOV, 0, 0xFF));
    cpu.step();
    EXPECT_EQ(cpu.r[0], 0xFFu);
    cpu.write16(2, f3(kF3CMP, 0, 0xFF));
    cpu.step();
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);
    // SUB r0, #1 -> 0xFE, no borrow.
    cpu.write16(4, f3(kF3SUB, 0, 1));
    cpu.step();
    EXPECT_EQ(cpu.r[0], 0xFEu);
    EXPECT_NE((cpu.cpsr & FLAG_C), 0u);   // C = NOT borrow
}

TEST(pipeline, cond_branch_target_from_plus_4) {
    ThumbCpu cpu;
    cpu.cpsr |= FLAG_Z;                   // EQ holds
    cpu.write16(0x100, bcond(0, -4));     // BEQ -4: target = 0x100+4-4
    cpu.write16(0x104, bany(2));          // would run if not taken
    cpu.r[15] = 0x100;
    const unsigned cyc = cpu.step();
    EXPECT_EQ(cyc, 3u);                   // taken branch refill
    EXPECT_EQ(cpu.r[15], 0x100u);
}

TEST(pipeline, bl_links_to_next_instruction) {
    ThumbCpu cpu;
    // BL with offset 0 lands at A+4 and LR = A+4.
    cpu.write16(0x100, static_cast<uint16_t>(0xF000));  // BL first half
    cpu.write16(0x102, static_cast<uint16_t>(0xF800));  // second half, lo=0
    cpu.r[15] = 0x100;
    cpu.step();                                          // first halfword
    cpu.step();                                          // second halfword
    EXPECT_EQ(cpu.r[14], 0x104u);
    EXPECT_EQ(cpu.r[15], 0x104u);
}

TEST(pipeline, alu_and_shift_ops) {
    ThumbCpu cpu;
    cpu.r[15] = 0;
    cpu.r[1] = 3;
    cpu.write16(0, f1(kLSL, 2, 1, 0));   // LSL r0, r1, #2
    cpu.step();
    EXPECT_EQ(cpu.r[0], 12u);
    cpu.write16(2, f4(12, 1, 0));        // ORR r0, r1
    cpu.step();
    EXPECT_EQ(cpu.r[0], 15u);
    cpu.write16(4, f3(kF3CMP, 0, 15));
    cpu.step();
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);
}

TEST(hidden, pipeline_hidden_semantics) {
    ThumbCpu cpu;
    // Countdown loop entirely in Thumb: MOV r0,#3; here: SUB #1; BNE back; B .
    cpu.r[15] = 0x80;
    cpu.write16(0x80, f3(kF3MOV, 0, 3));
    cpu.write16(0x82, f3(kF3SUB, 0, 1));            // sets flags
    cpu.write16(0x84, bcond(1, -6));                // BNE to 0x82
    cpu.write16(0x86, bany(-4));                    // B . self? target 0x86+4-4=0x86
    for (int i = 0; i < 20; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 0u);
    EXPECT_EQ(cpu.r[15], 0x86u);
}
