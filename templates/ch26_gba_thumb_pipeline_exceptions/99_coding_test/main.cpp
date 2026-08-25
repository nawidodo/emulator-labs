#define LABSTEST_MAIN
#include "labstest.hpp"
#include "coding_cpu.hpp"

using namespace coding;

static constexpr uint16_t f3(uint32_t op, uint32_t rd, uint32_t imm8) {
    return static_cast<uint16_t>(0x2000u | (op << 11) | (rd << 8) | imm8);
}
static constexpr uint16_t bany(int32_t byte_off) {
    return static_cast<uint16_t>(0xE000u | ((byte_off >> 1) & 0x7FF));
}
static constexpr uint16_t bcond(uint32_t cond, int32_t byte_off) {
    return static_cast<uint16_t>(0xD000u | (cond << 8) |
                                 ((byte_off >> 1) & 0xFF));
}

TEST(coding, bug1_backward_unconditional_branch_is_signed) {
    CodingCpu cpu;
    cpu.write16(0x20, f3(kF3MOV, 0, 0x5A));   // callee marker at 0x20
    cpu.write16(0x40, bany(-0x20));           // B to 0x22... target=0x40+4-32=0x24
    cpu.write16(0x24, bany(-4));              // park at 0x24
    cpu.write16(0x22, f3(kF3MOV, 1, 1));
    cpu.r[15] = 0x40;
    cpu.step();
    EXPECT_EQ(cpu.r[15], 0x24u);              // landed on the park loop
}

TEST(coding, bug2_cmp_does_not_writeback) {
    CodingCpu cpu;
    cpu.write16(0x00, f3(kF3MOV, 2, 7));      // r2 = 7
    cpu.step();
    cpu.write16(0x02, f3(kF3CMP, 2, 7));      // CMP r2, #7: equal
    cpu.step();
    EXPECT_EQ(cpu.r[2], 7u);                  // compare must not write
    EXPECT_NE((cpu.cpsr & FLAG_Z), 0u);
}

TEST(coding, bug3_literal_offset_is_word_scaled) {
    CodingCpu cpu;
    cpu.r[15] = 0x100;
    cpu.write32(0x108, 0xCAFEF00Du);          // literal: instr+4 + 1*4
    cpu.write32(0x110, 0xBAD0BAD0u);          // what an ARM +8 base reads
    cpu.write16(0x100, static_cast<uint16_t>(0x4800 | (3 << 8) | 1));
    cpu.step();
    EXPECT_EQ(cpu.r[3], 0xCAFEF00Du);
}

TEST(coding, bug4_taken_branch_costs_three_cycles) {
    CodingCpu cpu;
    cpu.write16(0x00, f3(kF3MOV, 0, 1));
    cpu.write16(0x02, bany(2));               // taken B over the next slot
    cpu.write16(0x06, bany(-4));              // park
    const unsigned c1 = cpu.step();           // MOV: 1S
    EXPECT_EQ(c1, 1u);
    const unsigned c2 = cpu.step();           // taken B: 2S+1N refill = 3
    EXPECT_EQ(c2, 3u);
}

TEST(coding, bug5_bl_links_past_the_pair) {
    CodingCpu cpu;
    const int32_t off = 0x50 - (0x40 + 4);    // forward call
    cpu.write16(0x40, static_cast<uint16_t>(0xF000 | ((off >> 12) & 0x7FF)));
    cpu.write16(0x42, static_cast<uint16_t>(0xF800 | ((off >> 1) & 0x7FF)));
    cpu.r[15] = 0x40;
    cpu.step();
    cpu.step();
    EXPECT_EQ(cpu.r[14], 0x44u);              // first_addr + 4
    EXPECT_EQ(cpu.r[15], 0x50u);
}

TEST(hidden, coding_hidden_trace_invariants) {
    // Countdown loop with a literal load; every PC and cycle count in the
    // trace of this program shape is hashed by the hidden grader.
    CodingCpu cpu;
    cpu.r[15] = 0x80;
    cpu.write16(0x80, f3(kF3MOV, 0, 2));      // r0 = 2
    cpu.write16(0x82, f3(kF3SUB, 0, 1));      // loop: r0 -= 1 (sets flags)
    cpu.write16(0x84, bcond(1, -6));          // BNE back to 0x82
    cpu.write16(0x86, static_cast<uint16_t>(0x4800 | (1 << 8) | 1));
    cpu.write16(0x88, bany(-4));              // park at 0x88
    cpu.write32(0x8E, 42);                    // literal for the LDR
    for (int i = 0; i < 10; ++i) cpu.step();
    EXPECT_EQ(cpu.r[0], 0u);
    EXPECT_EQ(cpu.r[1], 42u);
    EXPECT_EQ(cpu.r[15], 0x88u);
}
