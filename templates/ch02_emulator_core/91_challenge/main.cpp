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

// NOTE: labstest's EXPECT_EQ re-evaluates both arguments, so any call with
// side effects must be hoisted into a local first.
TEST(challenge, push_pop_roundtrip_is_lifo) {
    ch02::Cpu c;
    const auto e1 = c.push(0x11);
    const auto e2 = c.push(0x22);
    EXPECT_EQ(e1, ch02::StepError::None);
    EXPECT_EQ(e2, ch02::StepError::None);
    EXPECT_EQ(c.sp, 0xFD);       // pushes consumed 0xFF and 0xFE
    uint8_t v = 0;
    const auto p1 = c.pop(&v);
    EXPECT_EQ(p1, ch02::StepError::None);
    EXPECT_EQ(v, 0x22);          // last pushed, first popped
    const auto p2 = c.pop(&v);
    EXPECT_EQ(p2, ch02::StepError::None);
    EXPECT_EQ(v, 0x11);
    EXPECT_EQ(c.sp, 0xFF);       // empty again
}

TEST(challenge, pop_on_empty_stack_underflows) {
    ch02::Cpu c;
    uint8_t v = 0xEE;
    const auto err = c.pop(&v);
    EXPECT_EQ(err, ch02::StepError::StackUnderflow);
    EXPECT_EQ(v, 0xEE);          // untouched on failure
}

TEST(challenge, push_until_full_overflows) {
    ch02::Cpu c;
    for (int i = 0; i < 255; ++i)
        (void)c.push(1);
    // sp walked 0xFF -> 0x00; one more push would wrap into program space.
    const auto err = c.push(1);
    EXPECT_EQ(err, ch02::StepError::StackOverflow);
}

TEST(challenge, call_pushes_return_address_and_jumps) {
    // 0: CALL 0x06 ; 2: HALT ; ... ; 6: RET
    auto c = loaded({0x80, 0x06, 0x00,
                     0x00, 0x00, 0x00,
                     0x90});
    const uint32_t spent = c.run(100);
    EXPECT_TRUE(c.halted);           // RET resumed at the HALT at 0x02
    EXPECT_EQ(spent, 6u + 4u + 6u);
    EXPECT_EQ(c.pc, 3);
    EXPECT_EQ(c.sp, 0xFF);           // frame popped again
}

TEST(challenge, nested_calls_unwind_in_order) {
    // 0: CALL f1(0x10) ; 2: HALT ; f1@0x10: CALL f2(0x20) ; 0x12: RET ;
    // f2@0x20: RET
    auto c = loaded({
        0x80, 0x10, 0x00,                       // 0x00: CALL 0x10; HALT
        0x00, 0x00, 0x00, 0x00, 0x00,           // 0x03..0x07 pad
        0x00, 0x00, 0x00, 0x00, 0x00,           // 0x08..0x0C pad
        0x00, 0x00, 0x00,                       // 0x0D..0x0F pad
        0x80, 0x20,                             // 0x10: CALL 0x20
        0x90,                                   // 0x12: RET
        0x00, 0x00, 0x00, 0x00,                 // 0x13..0x16 pad
        0x00, 0x00, 0x00, 0x00,                 // 0x17..0x1A pad
        0x00, 0x00, 0x00, 0x00,                 // 0x1B..0x1E pad
        0x00,                                   // 0x1F pad
        0x90});                                 // 0x20: RET
    const uint32_t spent = c.run(100);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.pc, 3);              // back to the outermost resume point
    EXPECT_EQ(c.sp, 0xFF);
    EXPECT_EQ(spent, 28u);           // CALL+CALL+RET+RET = 24, HALT = 4
}

TEST(challenge, subroutine_works_from_two_call_sites) {
    // 0: LOAD r0,#5 ; 3: CALL double(0x12) ; 5: STORE r0,0x40 ;
    // 8: LOAD r0,#9 ; B: CALL double(0x12) ; D: HALT ; ...
    // 0x12: ADD r0,r0 (3 bytes!) ; 0x15: RET   ("double" the value in r0)
    auto c = loaded({0x10, 0x00, 0x05,
                     0x80, 0x12,
                     0x20, 0x00, 0x40,
                     0x10, 0x00, 0x09,
                     0x80, 0x12,
                     0x00,
                     0x00, 0x00, 0x00, 0x00,   // pad 0x0E..0x11
                     0x40, 0x00, 0x00,         // 0x12: ADD r0,r0
                     0x90});                   // 0x15: RET
    (void)c.run(100);
    EXPECT_EQ(c.ram[0x40], 10);      // first call doubled 5
    EXPECT_EQ(c.r[0], 18);           // second call doubled 9
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.sp, 0xFF);
}

TEST(challenge, ret_on_empty_stack_halts_with_underflow) {
    auto c = loaded({0x90});
    const ch02::StepResult res = c.step();
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(res.error, ch02::StepError::StackUnderflow);
    EXPECT_EQ(res.cycles, 6u);
}

TEST(challenge, unbounded_recursion_halts_with_overflow) {
    // addr 0: CALL 0x00 — recurses until the stack is exhausted.
    auto c = loaded({0x80, 0x00});
    ch02::StepResult res;
    for (int i = 0; i < 4096 && !c.halted; ++i)
        res = c.step();
    EXPECT_TRUE(c.halted);   // 256 pushes max: a healthy core halts way sooner
    EXPECT_EQ(res.error, ch02::StepError::StackOverflow);
    EXPECT_EQ(c.sp, 0x00);
}

TEST(challenge, flags_survive_call_ret) {
    // Raise carry+zero with ADD, then CALL an empty subroutine.
    // 0: LOAD r0,#255 ; 3: LOAD r1,#1 ; 6: ADD r0,r1 ; 9: CALL sub(0x0D) ;
    // B: HALT ; sub@0x0D: RET
    auto c = loaded({0x10, 0x00, 0xFF,
                     0x10, 0x01, 0x01,
                     0x40, 0x00, 0x01,
                     0x80, 0x0D,
                     0x00,
                     0x90});
    (void)c.run(100);
    EXPECT_TRUE(c.carry);
    EXPECT_TRUE(c.zero);
}
