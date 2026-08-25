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

// --- Bug #1 evidence: control flow ------------------------------------------

TEST(debug, jmp_lands_on_exact_target) {
    // 0: JMP 0x05 ; filler ; 5: LOAD r0,#9 ; 8: HALT
    auto c = loaded({0x60, 0x05, 0x99, 0x99, 0x99,
                     0x10, 0x00, 0x09, 0x00});
    const uint32_t spent = c.run(100);
    EXPECT_EQ(c.r[0], 9);        // a skewed jump lands in the filler instead
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(spent, 3u + 4u + 4u);
}

TEST(debug, forward_jump_chain_hits_both_targets) {
    // 0: LOAD r1,#1 ; 3: JMP 0x06 ; 5: poison ; 6: JMP 0x09 ; 8: poison ;
    // 9: HALT — every target must be hit exactly or poison gets executed.
    auto c = loaded({0x10, 0x01, 0x01,
                     0x60, 0x06, 0x99,
                     0x60, 0x09, 0x99,
                     0x00});
    const uint32_t spent = c.run(100);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(spent, 4u + 3u + 3u + 4u);
    EXPECT_EQ(c.pc, 10);         // a skewed jump dies in poison instead
}

TEST(debug, countdown_loop_terminates_with_exact_cycles) {
    // Same loop as exercise 03; a skewed backward JMP never reaches JZ's
    // target and the loop count goes wrong.
    auto c = loaded({0x10, 0x00, 0x07, 0x10, 0x01, 0x01,
                     0x50, 0x00, 0x01,
                     0x70, 0x0F,
                     0x60, 0x06,
                     0x00, 0x00,
                     0x00});
    const uint32_t spent = c.run(500);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.r[0], 0);
    EXPECT_EQ(spent, 73u);       // golden count from the reference core
}

// --- Bug #2 evidence: borrow flag --------------------------------------------

TEST(debug, sub_borrow_sets_carry_flag) {
    // LOAD r0,#4 ; LOAD r1,#9 ; SUB r0,r1 ; HALT -> borrow happened
    auto c = loaded({0x10, 0x00, 0x04, 0x10, 0x01, 0x09,
                     0x50, 0x00, 0x01, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.r[0], 251);
    EXPECT_TRUE(c.carry);        // SPEC.md: C is the BORROW flag
}

TEST(debug, sub_without_borrow_clears_carry_flag) {
    // LOAD r0,#9 ; LOAD r1,#4 ; SUB r0,r1 ; HALT -> no borrow
    auto c = loaded({0x10, 0x00, 0x09, 0x10, 0x01, 0x04,
                     0x50, 0x00, 0x01, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.r[0], 5);
    EXPECT_FALSE(c.carry);
}

TEST(debug, sub_equal_operands_zero_flag_only) {
    // Equal operands: result zero, NO borrow either way.
    auto c = loaded({0x10, 0x02, 0x55, 0x10, 0x03, 0x55,
                     0x50, 0x02, 0x03, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.r[2], 0);
    EXPECT_TRUE(c.zero);
    EXPECT_FALSE(c.carry);
}

// --- Sanity: unrelated behavior stays correct ---------------------------------

TEST(debug, add_and_load_still_correct) {
    auto c = loaded({0x10, 0x00, 0xFA, 0x10, 0x01, 0x0A,
                     0x40, 0x00, 0x01, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.r[0], 4);
    EXPECT_TRUE(c.carry);
}
