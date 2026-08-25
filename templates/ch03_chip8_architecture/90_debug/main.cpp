#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "chip8.hpp"

using chip8::Chip8;

// Regression tests for the reported register corruption. These encode the
// specification, not the current behavior: with the seeded bug present they
// fail, and they are the regression test half of your bug-report.

TEST(debug, ld_targets_register_from_high_nibble) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x6A, 0x42};  // LD VA, 0x42
    c.load(rom);
    c.step();
    EXPECT_EQ(c.v(10), 0x42);
    EXPECT_EQ(c.v(2), 0);   // low nibble of the word must NOT be targeted
}

TEST(debug, add_targets_register_from_high_nibble) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x7B, 0x03};  // ADD VB, 0x03
    c.load(rom);
    c.step();
    EXPECT_EQ(c.v(11), 0x03);
    EXPECT_EQ(c.v(3), 0);
}

TEST(debug, probe_program_final_state) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x00, 0xE0, 0x6A, 0x42, 0x7B, 0x03,
                           0xA2, 0x08, 0x12, 0x08};
    c.load(rom);
    for (int n = 0; n < 6; ++n) c.step();
    EXPECT_EQ(c.pc(), 0x208);  // parked in the self-jump
    EXPECT_EQ(c.v(10), 0x42);
    EXPECT_EQ(c.v(11), 0x03);
}
