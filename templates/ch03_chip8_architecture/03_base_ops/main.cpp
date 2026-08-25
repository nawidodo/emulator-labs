#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "chip8.hpp"

using chip8::Chip8;

// Tests drive state directly: load opcode bytes, step once, assert on machine
// state. Headless, no display, fully deterministic (§57).

TEST(base_ops, cls_clears_display) {
    Chip8 c;
    c.reset();
    for (auto& p : c.pixels()) p = 1;
    const uint8_t rom[] = {0x00, 0xE0};
    c.load(rom);
    c.step();
    bool any_lit = false;
    for (const auto& p : c.pixels())
        if (p) any_lit = true;
    EXPECT_FALSE(any_lit);
}

TEST(base_ops, jp_sets_pc_to_nnn) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x12, 0x14};  // JP 0x214
    c.load(rom);
    c.step();
    EXPECT_EQ(c.pc(), 0x214);
}

TEST(base_ops, ld_vx_nn_loads_correct_register) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x6A, 0x42};  // LD VA, 0x42
    c.load(rom);
    c.step();
    EXPECT_EQ(c.v(10), 0x42);  // X = bits 11..8 (the A in 6A42)
    EXPECT_EQ(c.v(2), 0);      // a wrong-nibble decode lights this one up
    EXPECT_EQ(c.v(6), 0);
}

TEST(base_ops, add_vx_nn_wraps_at_8_bits) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x65, 0xE7, 0x75, 0x2A};  // V5=E7; V5 += 2A
    c.load(rom);
    c.step();
    c.step();
    EXPECT_EQ(c.v(5), 0x11);  // 0xE7 + 0x2A = 0x111 -> wraps to 0x11
}

TEST(base_ops, ld_i_nnn) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0xA3, 0x4B};  // LD I, 0x34B
    c.load(rom);
    c.step();
    EXPECT_EQ(c.i(), 0x34B);
}

TEST(base_ops, unknown_opcode_is_nop_but_advances_pc) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x90, 0x90};  // implemented only in chapter 4
    c.load(rom);
    auto r = c.step();
    EXPECT_EQ(r.pc, 0x202);
    EXPECT_EQ(c.v(9), 0);
    EXPECT_EQ(c.i(), 0);
}

// End-to-end micro-program, identical to tests/public base_demo.ch8:
//   CLS; V0=42; V1=17; V5=E7; V5+=2A; I=216; JP 210; NOP; 210: JP 210
TEST(base_ops, demo_program_reaches_steady_state) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x00, 0xE0, 0x60, 0x42, 0x71, 0x17, 0x65, 0xE7,
                           0x75, 0x2A, 0xA2, 0x16, 0x12, 0x10, 0x00, 0x00,
                           0x12, 0x10};
    c.load(rom);
    for (int n = 0; n < 12; ++n) c.step();
    EXPECT_EQ(c.pc(), 0x210);  // parked in the self-jump
    EXPECT_EQ(c.v(0), 0x42);
    EXPECT_EQ(c.v(1), 0x17);
    EXPECT_EQ(c.v(5), 0x11);
    EXPECT_EQ(c.i(), 0x216);
}
