#define LABSTEST_MAIN
#include "labstest.hpp"

#include <span>

#include "cpu.hpp"

namespace {

ch02::Cpu loaded(std::initializer_list<uint8_t> program) {
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(program.begin(), program.size()));
    return c;
}

}  // namespace

TEST(fde, load_then_halt_runs_full_program) {
    // LOAD r0,#7 ; HALT
    auto c = loaded({0x10, 0x00, 0x07, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(spent, 8u);       // 4 + 4
    EXPECT_EQ(c.r[0], 7);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.pc, 4);
}

TEST(fde, fetch_leaves_pc_at_next_instruction) {
    // LOAD r1,#1 at address 0 is 3 bytes wide.
    auto c = loaded({0x10, 0x01, 0x01, 0x00});
    const ch02::StepResult res = c.step();
    EXPECT_EQ(res.error, ch02::StepError::None);
    EXPECT_EQ(res.cycles, 4u);
    EXPECT_EQ(res.pc, 3);
    EXPECT_EQ(c.pc, 3);
    EXPECT_EQ(c.r[1], 1);
}

TEST(fde, operand_fetch_wraps_past_0xff) {
    // JMP 0xFF placed so its single operand byte sits at 0xFF... instead put
    // LOAD r2,#0x5A at 0xFE: opcode at 0xFE, register byte wraps to 0x00,
    // immediate wraps to 0x01.
    auto c = loaded({/* pad */});
    c.ram[0xFE] = 0x10;  // LOAD
    c.ram[0xFF] = 0x02;  // r2
    c.ram[0x00] = 0x5A;  // immediate (wrapped)
    c.pc = 0xFE;
    const ch02::StepResult res = c.step();
    EXPECT_EQ(res.error, ch02::StepError::None);
    EXPECT_EQ(c.r[2], 0x5A);
    EXPECT_EQ(c.pc, 0x01);      // wrapped past the end
}

TEST(fde, unknown_opcode_halts_deterministically) {
    auto c = loaded({0x99, 0xEE});
    const ch02::StepResult res = c.step();
    EXPECT_EQ(res.error, ch02::StepError::UnknownOpcode);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.cycles, 2u);  // opcode + abort
    EXPECT_EQ(res.pc, 1);       // consumed only the opcode byte
}

TEST(fde, bad_register_field_halts) {
    // LOAD with reserved register field 5: fetched fully, rejected in decode.
    auto c = loaded({0x10, 0x05, 0x42, 0x00});
    const ch02::StepResult res = c.step();
    EXPECT_EQ(res.error, ch02::StepError::BadRegister);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.cycles, 4u);
    EXPECT_EQ(res.pc, 3);       // pc already advanced past the instruction
}

TEST(fde, run_accumulates_until_halt_even_with_big_budget) {
    // LOAD + HALT: run() returns the exact spend long before the budget.
    auto c = loaded({0x10, 0x00, 0x07, 0x00});
    EXPECT_EQ(c.run(10000), 8u);
}

TEST(fde, stepping_a_halted_machine_is_a_noop) {
    auto c = loaded({0x00});
    (void)c.step();
    EXPECT_TRUE(c.halted);
    const ch02::StepResult res = c.step();
    EXPECT_EQ(res.cycles, 0u);
    EXPECT_EQ(res.pc, c.pc);
}

TEST(fde, reset_clears_all_state) {
    auto c = loaded({0x10, 0x00, 0x09, 0x00});
    (void)c.run(100);
    c.reset();
    EXPECT_EQ(c.r[0], 0);
    EXPECT_FALSE(c.halted);
    EXPECT_EQ(c.pc, 0);
    EXPECT_EQ(c.ram[0], 0);
}
