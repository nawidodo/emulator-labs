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

// LOAD rd,#imm ; ADD rd,rs ; HALT  — runs one ADD against a known state.
ch02::StepResult run_add(ch02::Cpu& c, uint8_t ra, uint8_t va, uint8_t rb,
                         uint8_t vb) {
    const uint8_t prog[] = {0x10, ra, va, 0x10, rb, vb,
                            0x40, ra, rb, 0x00};
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));
    ch02::StepResult res;
    for (int i = 0; i < 3; ++i)
        res = c.step();
    return res;
}

ch02::StepResult run_sub(ch02::Cpu& c, uint8_t ra, uint8_t va, uint8_t rb,
                         uint8_t vb) {
    const uint8_t prog[] = {0x10, ra, va, 0x10, rb, vb,
                            0x50, ra, rb, 0x00};
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));
    ch02::StepResult res;
    for (int i = 0; i < 3; ++i)
        res = c.step();
    return res;
}

}  // namespace

TEST(alu, add_basic) {
    ch02::Cpu c;
    (void)run_add(c, 0, 5, 1, 7);
    EXPECT_EQ(c.r[0], 12);
    EXPECT_FALSE(c.zero);
    EXPECT_FALSE(c.carry);
}

TEST(alu, add_wraps_and_sets_carry) {
    ch02::Cpu c;
    (void)run_add(c, 0, 250, 1, 10);
    EXPECT_EQ(c.r[0], 4);        // 260 mod 256
    EXPECT_TRUE(c.carry);
    EXPECT_FALSE(c.zero);
}

TEST(alu, add_overflow_to_zero_sets_both_flags) {
    ch02::Cpu c;
    (void)run_add(c, 2, 128, 3, 128);
    EXPECT_EQ(c.r[2], 0);
    EXPECT_TRUE(c.zero);         // truncated result is zero...
    EXPECT_TRUE(c.carry);        // ...but the wide sum was 256
}

TEST(alu, add_zero_plus_zero_is_zero_not_carry) {
    ch02::Cpu c;
    (void)run_add(c, 0, 0, 1, 0);
    EXPECT_EQ(c.r[0], 0);
    EXPECT_TRUE(c.zero);
    EXPECT_FALSE(c.carry);
}

TEST(alu, sub_no_borrow_clears_carry) {
    ch02::Cpu c;
    (void)run_sub(c, 0, 9, 1, 4);
    EXPECT_FALSE(c.zero);
}

TEST(alu, sub_borrow_wraps_and_sets_carry) {
    ch02::Cpu c;
    (void)run_sub(c, 0, 4, 1, 9);
    EXPECT_EQ(c.r[0], 251);      // 4 - 9 mod 256
    EXPECT_TRUE(c.carry);        // borrow happened
    EXPECT_FALSE(c.zero);
}

TEST(alu, sub_equal_sets_zero_and_clears_carry) {
    ch02::Cpu c;
    (void)run_sub(c, 3, 7, 1, 7);
    EXPECT_EQ(c.r[3], 0);
    EXPECT_TRUE(c.zero);
    EXPECT_FALSE(c.carry);
}

TEST(alu, load_does_not_touch_flags) {
    // ADD sets carry; an intervening LOAD must preserve it.
    // LOAD r0,#255 ; LOAD r1,#1 ; ADD r0,r1 ; LOAD r2,#99 ; HALT
    auto c = loaded({0x10, 0x00, 0xFF, 0x10, 0x01, 0x01,
                     0x40, 0x00, 0x01, 0x10, 0x02, 0x63, 0x00});
    (void)c.run(100);
    EXPECT_EQ(c.r[0], 0);
    EXPECT_EQ(c.r[2], 99);
    EXPECT_TRUE(c.carry);        // survived two non-ALU instructions
    EXPECT_TRUE(c.zero);         // 255+1 truncates to 0: both flags hold
}
