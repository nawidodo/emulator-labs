#define LABSTEST_MAIN
#include "labstest.hpp"
#include "chip8.hpp"

using chip8::Chip8;
using chip8::kFontAddr;
using chip8::kProgStart;

TEST(state, reset_installs_fontset_at_0x050) {
    Chip8 c;
    c.reset();
    // Digit '0' sprite: F0 90 90 90 F0.
    EXPECT_EQ(c.mem(0x050), 0xF0);
    EXPECT_EQ(c.mem(0x051), 0x90);
    EXPECT_EQ(c.mem(0x054), 0xF0);
    // Last byte of the font (digit 'F', final row).
    EXPECT_EQ(c.mem(kFontAddr + 79), 0x80);
}

TEST(state, reset_clears_memory_and_state) {
    Chip8 c;
    const uint8_t rom[] = {0xAA, 0xBB, 0xCC};
    c.load(rom);
    c.reset();
    // ROM bytes must be gone: reset wipes memory before reinstalling font.
    EXPECT_EQ(c.mem(kProgStart), 0x00);
    EXPECT_EQ(c.mem(kProgStart + 1), 0x00);
    // Font survived the wipe.
    EXPECT_EQ(c.mem(0x050), 0xF0);
}

TEST(state, reset_defaults_registers_and_timers) {
    Chip8 c;
    c.reset();
    EXPECT_EQ(c.pc(), kProgStart);
    EXPECT_EQ(c.i(), 0);
    EXPECT_EQ(c.sp(), 0);
    EXPECT_EQ(c.delay(), 0);
    EXPECT_EQ(c.sound(), 0);
}

TEST(state, load_places_rom_at_0x200) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x00, 0xE0, 0x12, 0x34};
    c.load(rom);
    EXPECT_EQ(c.mem(kProgStart), 0x00);
    EXPECT_EQ(c.mem(kProgStart + 1), 0xE0);
    EXPECT_EQ(c.mem(kProgStart + 2), 0x12);
    EXPECT_EQ(c.mem(kProgStart + 3), 0x34);
}

TEST(state, load_leaves_font_intact) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0xFF, 0xFF};
    c.load(rom);
    EXPECT_EQ(c.mem(0x050), 0xF0);
    EXPECT_EQ(c.mem(kFontAddr + 79), 0x80);
}
