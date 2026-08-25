#define LABSTEST_MAIN
#include "labstest.hpp"
#include "coding_cpu.hpp"

using namespace arm;

// Encodings per CODING_TEST.md.
static constexpr uint32_t swap(bool byte, uint32_t rn, uint32_t rd,
                               uint32_t rm) {
    return 0xE1000090u | (byte ? 1u << 22 : 0) | (rn << 16) | (rd << 12) | rm;
}
static constexpr uint32_t mrs(bool spsr, uint32_t rd) {
    return 0xE10F0000u | (spsr ? 1u << 22 : 0) | (rd << 12);
}
// mask: bit3 = f (flags), bit0 = c (control)
static constexpr uint32_t msr_imm(bool spsr, uint32_t mask, uint32_t rot,
                                  uint32_t imm8) {
    return 0xE320F000u | (spsr ? 1u << 22 : 0) |
           ((mask & 8) ? 1u << 19 : 0) | ((mask & 1) ? 1u << 16 : 0) |
           (rot << 8) | imm8;
}
static constexpr uint32_t msr_reg(bool spsr, uint32_t mask, uint32_t rm) {
    return 0xE120F000u | (spsr ? 1u << 22 : 0) |
           ((mask & 8) ? 1u << 19 : 0) | ((mask & 1) ? 1u << 16 : 0) | rm;
}

TEST(coding, swp_word_roundtrip) {
    CodingTestCpu cpu;
    cpu.write32(0x800, 0x11223344u);
    cpu.r[1] = 0x800;
    cpu.r[2] = 0xAABBCCDDu;
    EXPECT_TRUE(cpu.maybe_exec_status(swap(false, 1, 3, 2)));
    EXPECT_EQ(cpu.r[3], 0x11223344u);   // old value delivered
    EXPECT_EQ(cpu.read32(0x800), 0xAABBCCDDu);
}

TEST(coding, swpb_zero_extends_old_byte) {
    CodingTestCpu cpu;
    cpu.mem[0x900] = 0x7E;
    cpu.r[1] = 0x900;
    cpu.r[2] = 0xFFFF00AAu;
    cpu.maybe_exec_status(swap(true, 1, 4, 2));
    EXPECT_EQ(cpu.r[4], 0x7Eu);
    EXPECT_EQ(cpu.mem[0x900], 0xAA);
}

TEST(coding, mrs_reads_cpsr_and_spsr) {
    CodingTestCpu cpu;
    cpu.cpsr = 0xD0000013u;
    cpu.spsr = 0x40000010u;
    cpu.maybe_exec_status(mrs(false, 5));
    EXPECT_EQ(cpu.r[5], 0xD0000013u);
    cpu.maybe_exec_status(mrs(true, 6));
    EXPECT_EQ(cpu.r[6], 0x40000010u);
}

TEST(coding, msr_masked_fields_only) {
    CodingTestCpu cpu;
    cpu.cpsr = 0x00000013u;
    // MSR CPSR_f, #0xD0000000 == ror(0x0D, 4): all four flags set.
    cpu.maybe_exec_status(msr_imm(false, 8, 2, 0x0D));
    EXPECT_EQ((cpu.cpsr >> 28), 0xDu);
    EXPECT_EQ((cpu.cpsr & 0xFF), 0x13u);   // control byte untouched
    // MSR CPSR_c, #imm keeps the flag byte.
    const uint32_t before = cpu.cpsr & 0xF0000000u;
    cpu.maybe_exec_status(msr_imm(false, 1, 0, 0x5F));
    EXPECT_EQ((cpu.cpsr & 0xFF), 0x5Fu);
    EXPECT_EQ((cpu.cpsr & 0xF0000000u), before);
}

TEST(coding, msr_register_form_targets_spsr) {
    CodingTestCpu cpu;
    cpu.r[2] = 0xC0000000u;
    cpu.spsr = 0;
    cpu.cpsr = 0x10;
    EXPECT_TRUE(cpu.maybe_exec_status(msr_reg(true, 8, 2)));
    EXPECT_EQ(cpu.spsr, 0xC0000000u);
    EXPECT_EQ(cpu.cpsr, 0x10u);            // CPSR untouched when SPSR selected
}

TEST(coding, non_family_instructions_pass_through) {
    CodingTestCpu cpu;
    cpu.r[0] = 0;
    EXPECT_FALSE(cpu.maybe_exec_status(0xE3A00042u));   // plain MOV imm
    EXPECT_FALSE(cpu.maybe_exec_status(0xEAFFFFFEu));   // branch
}

TEST(hidden, coding_hidden_family_semantics) {
    CodingTestCpu cpu;
    // SWP leaves flags untouched.
    cpu.write32(0xA00, 5);
    cpu.r[1] = 0xA00;
    cpu.r[2] = 9;
    const uint32_t cpsr_before = cpu.cpsr;
    cpu.maybe_exec_status(swap(false, 1, 0, 2));
    EXPECT_EQ(cpu.cpsr, cpsr_before);
    EXPECT_EQ(cpu.r[0], 5u);
    // MRS after MSR round-trips the full word.
    cpu.maybe_exec_status(msr_imm(false, 8, 0, 0));     // flags <- 0
    EXPECT_EQ((cpu.cpsr >> 28), 0u);
    cpu.maybe_exec_status(mrs(false, 14));              // MRS r14, CPSR
    EXPECT_EQ(cpu.r[14], cpu.cpsr);
}
