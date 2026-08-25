#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_cpu.hpp"

using namespace debugcore;

static constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x2000u | (op << 11) | (rd << 8) | imm8);
}
static constexpr uint16_t bcond(uint32_t cond, int32_t byte_off) {
    return static_cast<uint16_t>(0xD000u | (cond << 8) |
                                 ((byte_off >> 1) & 0xFF));
}
static constexpr uint16_t bany(int32_t byte_off) {
    return static_cast<uint16_t>(0xE000u | ((byte_off >> 1) & 0x7FF));
}

// One test per seeded defect; each names the symptom from DEBUGGING.md.
TEST(debug_suite, bug1_thumb_fetch_stride_is_two) {
    DebugCpu cpu;
    // Three MOVs in a row: every halfword must execute.
    cpu.write16(0x00, f3(kF3MOV, 0, 1));
    cpu.write16(0x02, f3(kF3MOV, 1, 2));
    cpu.write16(0x04, f3(kF3MOV, 2, 3));
    for (int i = 0; i < 6; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 1u);
    EXPECT_EQ(cpu.r[1], 2u);
    EXPECT_EQ(cpu.r[2], 3u);
}

TEST(debug_suite, bug2_literal_pool_base_is_instr_plus_4) {
    DebugCpu cpu;
    cpu.r[15] = 0x100;
    cpu.write32(0x108, 0xDEADBEEFu);   // literal at instr+4 + imm*4 (imm=1)
    cpu.write32(0x10C, 0x11111111u);   // what a wrong base would read
    cpu.write16(0x100, static_cast<uint16_t>(0x4800 | (0 << 8) | 1)); // LDR r0,[PC,#4]
    cpu.step();
    EXPECT_EQ(cpu.r[0], 0xDEADBEEFu);
}

TEST(debug_suite, bug3_cond_branch_target_from_plus_4) {
    DebugCpu cpu;
    cpu.cpsr |= FLAG_Z;
    // BEQ -8 at 0x100 must land exactly at 0xFC.
    cpu.write16(0xFC, bany(-2));       // park loop if reached
    cpu.write16(0xFE, bany(-2));
    cpu.write16(0x100, bcond(0, -8));  // target 0xFC... wait: 0x100+4-8 = 0xFC
    cpu.r[15] = 0x100;
    cpu.step();
    EXPECT_EQ(cpu.r[15], 0xFCu);
}

TEST(debug_suite, bug4_bl_backward_offset_is_sign_extended) {
    DebugCpu cpu;
    // BL backward from 0x40 to 0x20: offset -0x20 -> high bit pattern set.
    const int32_t off = 0x20 - (0x40 + 4);            // -0x24
    cpu.write16(0x40, static_cast<uint16_t>(0xF000 | ((off >> 12) & 0x7FF)));
    cpu.write16(0x42, static_cast<uint16_t>(0xF800 | ((off >> 1) & 0x7FF)));
    cpu.write16(0x20, f3(kF3MOV, 5, 0x77));           // callee marker
    cpu.r[15] = 0x40;
    cpu.step();                                        // first halfword
    cpu.step();                                        // second: branch taken
    EXPECT_EQ(cpu.r[14], 0x44u);
    EXPECT_EQ(cpu.r[15], 0x20u);
    cpu.step();
    EXPECT_EQ(cpu.r[5], 0x77u);                        // really landed there
}

TEST(debug_suite, bug5_swi_return_restores_cpsr) {
    DebugCpu cpu;
    cpu.cpsr |= FLAG_C | FLAG_Z;
    cpu.write16(0x00, static_cast<uint16_t>(0xDF42));  // SWI 0x42
    cpu.step();
    EXPECT_EQ(cpu.r[15], 0x08u);                       // vectored to handler
    EXPECT_NE(cpu.spsr_svc, 0u);                       // state was saved
    cpu.exception_return();
    EXPECT_EQ(cpu.r[15], 0x04u);
    EXPECT_EQ((cpu.cpsr & (FLAG_C | FLAG_Z)), FLAG_C | FLAG_Z);
    EXPECT_EQ(cpu.cpsr & 0x1F, 0x10u);                 // user mode back
}

TEST(hidden, debug_hidden_pipeline_invariants) {
    // Combined program: countdown loop + literal load.
    DebugCpu cpu;
    cpu.r[15] = 0x80;
    cpu.write16(0x80, f3(kF3MOV, 0, 3));
    cpu.write16(0x82, f3(kF3SUB, 0, 1));
    cpu.write16(0x84, bcond(1, -6));                   // BNE to 0x82
    cpu.write16(0x86, bany(-4));                       // B self
    for (int i = 0; i < 30; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 0u);
    EXPECT_EQ(cpu.r[15], 0x86u);
    cpu.r[15] = 0xA0;
    cpu.write32(0xA8, 42);                             // literal at A0+4+4
    cpu.write16(0xA0, static_cast<uint16_t>(0x4800 | (1 << 8) | 1));
    cpu.step();
    EXPECT_EQ(cpu.r[1], 42u);
}
