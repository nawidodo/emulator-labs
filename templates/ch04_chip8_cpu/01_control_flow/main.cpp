#define LABSTEST_MAIN
#include "labstest.hpp"
#include "chip8.hpp"

#include <vector>

namespace {

using chip8::Chip8;

Chip8 mk(const std::vector<uint8_t>& prog) {
    Chip8 c;
    c.reset();
    c.load(prog);
    return c;
}

void run(Chip8& c, int n) {
    for (int i = 0; i < n && !c.halted; ++i) c.step();
}

}  // namespace

// This exercise's ISA is control flow ONLY (no loads yet), so observable
// state is PC + stack; registers are preset directly where a test needs
// specific comparison values.

// 1NNN lands at the target and keeps executing from there.
TEST(flow, jp_1nnn) {
    // 0200: JP 0x208 ; 0208: JP 0x20C ; 0x20C: illegal -> halt.
    std::vector<uint8_t> prog(12, 0);
    prog[0] = 0x12; prog[1] = 0x08;
    prog[8] = 0x12; prog[9] = 0x0C;
    Chip8 c = mk(prog);
    run(c, 3);
    EXPECT_EQ(c.halted, true);
    EXPECT_EQ(c.pc, 0x20E);
}

// 2NNN pushes the return address (PC AFTER the call); 00EE pops it.
TEST(flow, call_ret_roundtrip) {
    std::vector<uint8_t> prog(10, 0);
    prog[0] = 0x22; prog[1] = 0x08;   // 0200 CALL 0x208
    prog[2] = 0x30; prog[3] = 0x00;   // 0202 SE V0,0 (executes after RET)
    prog[8] = 0x00; prog[9] = 0xEE;   // 0208 RET
    Chip8 c = mk(prog);
    run(c, 3);                        // CALL, RET, SE
    EXPECT_EQ(c.sp, 0);
    EXPECT_EQ(c.pc, 0x206);           // SE skipped its successor
    EXPECT_FALSE(c.halted);
}

// Nested calls keep the stack consistent through LIFO returns.
TEST(flow, nested_calls) {
    std::vector<uint8_t> prog(16, 0);
    prog[0]  = 0x22; prog[1]  = 0x0A; // 0200 CALL f1(0x20A)
    prog[2]  = 0x50; prog[3]  = 0x00; // 0202 SE V1,V0 (after returns)
    prog[10] = 0x22; prog[11] = 0x0E; // 020A f1: CALL f2(0x20E)
    prog[12] = 0x00; prog[13] = 0xEE; // 020C f1: RET
    prog[14] = 0x00; prog[15] = 0xEE; // 020E f2: RET
    Chip8 c = mk(prog);
    run(c, 5);                        // CALL,CALL,RET,RET,SE
    EXPECT_EQ(c.sp, 0);
    EXPECT_EQ(c.pc, 0x206);
}

// Stack overflow faults instead of corrupting memory.
TEST(flow, stack_overflow_halts) {
    std::vector<uint8_t> prog(32);
    for (size_t k = 0; k < prog.size(); k += 2) {
        prog[k] = 0x22;      // CALL 0x204 (self-referential recursion)
        prog[k + 1] = 0x04;
    }
    Chip8 c = mk(prog);
    run(c, 40);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.sp, Chip8::kStackSlots);
}

// BNNN jumps to NNN + V0.
TEST(flow, bnnn_jumps_by_v0) {
    std::vector<uint8_t> prog(0x114, 0);
    prog[0] = 0xB3; prog[1] = 0x0A;          // 0200 JP V0+0x30A -> 0x30F
    prog[0x10F] = 0x13; prog[0x110] = 0x14;  // 030F JP 0x314
    Chip8 c = mk(prog);
    c.v[0] = 5;
    run(c, 2);              // BNNN, then the JP sitting at the target
    EXPECT_EQ(c.pc, 0x314);                  // jump target of the JP at 0x30F
}

// BNNN arithmetic wraps in the 12-bit address space.
TEST(flow, bnnn_wraps_12bit) {
    std::vector<uint8_t> prog(16, 0);
    prog[0] = 0xBF; prog[1] = 0xFF;          // -> 0xFFF + 0x010 = wrap 0x00F
    Chip8 c = mk(prog);
    c.v[0] = 0x10;
    run(c, 1);
    EXPECT_EQ(c.pc, 0x00F);
}

// 3XNN skips the next instruction on equality (register preset).
TEST(flow, se_imm_skips_on_equal) {
    std::vector<uint8_t> prog(8, 0);
    prog[0] = 0x31; prog[1] = 0x07;   // SE V1,#7
    prog[4] = 0x12; prog[5] = 0x0A;   // witness would sit at 0x204
    Chip8 taken = mk(prog);
    taken.v[1] = 7;
    run(taken, 1);
    EXPECT_EQ(taken.pc, 0x204);

    Chip8 not_taken = mk(prog);
    not_taken.v[1] = 9;
    run(not_taken, 1);
    EXPECT_EQ(not_taken.pc, 0x202);
}

// 4XNN skips on inequality.
TEST(flow, sne_imm_skips_on_not_equal) {
    std::vector<uint8_t> prog(8, 0);
    prog[0] = 0x41; prog[1] = 0x99;   // SNE V1,#0x99
    Chip8 taken = mk(prog);
    taken.v[1] = 0x42;
    run(taken, 1);
    EXPECT_EQ(taken.pc, 0x204);

    Chip8 not_taken = mk(prog);
    not_taken.v[1] = 0x99;
    run(not_taken, 1);
    EXPECT_EQ(not_taken.pc, 0x202);
}

// Register-form skips: 5XY0 equal->skip, 9XY0 not-equal->skip.
TEST(flow, se_reg_and_sne_reg) {
    std::vector<uint8_t> se(6, 0);
    se[0] = 0x51; se[1] = 0x20;       // SE V1,V2
    Chip8 a = mk(se);
    a.v[1] = 5; a.v[2] = 5;
    run(a, 1);
    EXPECT_EQ(a.pc, 0x204);

    Chip8 b = mk(se);
    b.v[1] = 5; b.v[2] = 6;
    run(b, 1);
    EXPECT_EQ(b.pc, 0x202);

    std::vector<uint8_t> sne(6, 0);
    sne[0] = 0x91; sne[1] = 0x20;     // SNE V1,V2
    Chip8 c = mk(sne);
    c.v[1] = 5; c.v[2] = 6;
    run(c, 1);
    EXPECT_EQ(c.pc, 0x204);
}

// Fetch and skip arithmetic must both wrap inside the 12-bit address space.
TEST(flow, fetch_and_skip_wrap_12bit) {
    Chip8 c;
    c.reset();
    // Opcode spanning the boundary: mem[0xFFF]=0x33, mem[wrap(0x1000)=0x000]
    // = font byte 0xF0 -> opcode 0x33F0 = SE V3,0xF0.
    c.mem[0xFFF] = 0x33;
    c.v[3] = 0xF0;
    c.pc = 0xFFF;
    c.step();
    EXPECT_EQ(c.cycles, 1u);       // executed exactly one wrapped instruction
    EXPECT_EQ(c.pc, 0x003);        // fetch advanced to 0x001, skip added +2
}

// Unimplemented/illegal opcodes halt with the opcode recorded (trace-first).
TEST(flow, illegal_opcode_halts) {
    Chip8 c = mk({0xD1, 0x23});  // display sprite: ch05 territory, illegal here
    run(c, 1);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.illegal_op, 0xD123);
}
