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

// MATH-1 8XY8 AVG: integer average, VF = lost low bit.
TEST(ext, avg) {
    const uint8_t p[] = {0x81, 0x28};
    Chip8 a = mk(std::vector<uint8_t>(p, p + 2));
    a.v[1] = 7; a.v[2] = 4;
    run(a, 1);
    EXPECT_EQ(a.v[1], 5);        // (7+4)/2 = 5
    EXPECT_EQ(a.v[0xF], 1);      // sum odd -> flag

    Chip8 b = mk(std::vector<uint8_t>(p, p + 2));
    b.v[1] = 200; b.v[2] = 100;  // sum 300 -> avg 150, low bit 0
    run(b, 1);
    EXPECT_EQ(b.v[1], 150);
    EXPECT_EQ(b.v[0xF], 0);
}

// MATH-2 8XY9 MIN.
TEST(ext, min) {
    const uint8_t p[] = {0x81, 0x29};
    Chip8 a = mk(std::vector<uint8_t>(p, p + 2));
    a.v[1] = 9; a.v[2] = 3;
    run(a, 1);
    EXPECT_EQ(a.v[1], 3);
    EXPECT_EQ(a.v[0xF], 1);      // replacement happened

    Chip8 b = mk(std::vector<uint8_t>(p, p + 2));
    b.v[1] = 3; b.v[2] = 9;
    run(b, 1);
    EXPECT_EQ(b.v[1], 3);
    EXPECT_EQ(b.v[0xF], 0);

    Chip8 c = mk(std::vector<uint8_t>(p, p + 2));  // equal: no replacement
    c.v[1] = 5; c.v[2] = 5;
    run(c, 1);
    EXPECT_EQ(c.v[0xF], 0);
}

// MATH-3 8XYA MAX.
TEST(ext, max) {
    const uint8_t p[] = {0x81, 0x2A};
    Chip8 a = mk(std::vector<uint8_t>(p, p + 2));
    a.v[1] = 9; a.v[2] = 30;
    run(a, 1);
    EXPECT_EQ(a.v[1], 30);
    EXPECT_EQ(a.v[0xF], 1);

    Chip8 b = mk(std::vector<uint8_t>(p, p + 2));
    b.v[1] = 30; b.v[2] = 9;
    run(b, 1);
    EXPECT_EQ(b.v[1], 30);
    EXPECT_EQ(b.v[0xF], 0);
}

// MATH-4 8XYB MULLO: low byte in VX, high byte in VF. FF*FF=FE01 boundary.
TEST(ext, mullo) {
    const uint8_t p[] = {0x81, 0x2B};
    Chip8 a = mk(std::vector<uint8_t>(p, p + 2));
    a.v[1] = 0xFF; a.v[2] = 0xFF;
    run(a, 1);
    EXPECT_EQ(a.v[1], 0x01);
    EXPECT_EQ(a.v[0xF], 0xFE);

    Chip8 b = mk(std::vector<uint8_t>(p, p + 2));
    b.v[1] = 16; b.v[2] = 4;     // 64: fits in one byte
    run(b, 1);
    EXPECT_EQ(b.v[1], 64);
    EXPECT_EQ(b.v[0xF], 0);
}

// MATH-5 FXY2 XSUM: XOR-fold [I, I+VY]; I unchanged; VF cleared. Wraps at 4K.
TEST(ext, xsum) {
    // memory at I: three bytes 0xA5 0x5A 0xFF -> fold = A5^5A^FF = 0x0A? check:
    // A5^5A = FF; FF^FF = 00. Use distinct values instead.
    const uint8_t p[] = {0xA3, 0x00,   // I = 0x300
                         0x62, 0x02,   // V2 = 2 (range of 3 bytes)
                         0xF3, 0xF2};  // XSUM V3 <- [300..302]
    Chip8 c = mk(std::vector<uint8_t>(p, p + 6));
    c.mem[0x300] = 0xA5;
    c.mem[0x301] = 0x0F;
    c.mem[0x302] = 0x70;
    c.v[0xF] = 1;                // must be cleared by the extension
    run(c, 3);
    EXPECT_EQ(c.v[3], uint8_t(0xA5 ^ 0x0F ^ 0x70));
    EXPECT_EQ(c.idx, 0x300);     // untouched
    EXPECT_EQ(c.v[0xF], 0);
}

// Program-level: chained extension ops straight from the spec text.
TEST(codingtest, extension_program) {
    Chip8 c = mk({0x61, 0xC8,
                  0x62, 0x32,
                  0x81, 0x28,   // AVG   V1 = (C8+32)/2 = 7D, VF = 0
                  0x63, 0xE9,
                  0x83, 0x19,   // MIN   V3 <- V1 : 7D < E9 -> V3 = 7D, VF = 1
                  0x84, 0x2A,   // MAX   V4 <- V2 : 32 > 00 -> V4 = 32, VF = 1
                  0x61, 0x10,
                  0x62, 0x10,
                  0x81, 0x2B,   // MULLO 10*10 = 0100 -> V1 = 00, VF = 01
                  0x00, 0x00});
    run(c, 10);                        // nine ops + the trailing halt opcode
    EXPECT_EQ(c.halted, true);
    EXPECT_EQ(c.v[3], 0x7D);
    EXPECT_EQ(c.v[4], 0x32);
    EXPECT_EQ(c.v[1], 0x00);
    EXPECT_EQ(c.v[0xF], 0x01);
}

// Unassigned extension codes must still halt the machine.
TEST(codingtest, unassigned_ext_code_halts) {
    Chip8 c = mk({0x81, 0x2C});  // 8XYC intentionally left unassigned
    run(c, 1);
    EXPECT_TRUE(c.halted);
}
