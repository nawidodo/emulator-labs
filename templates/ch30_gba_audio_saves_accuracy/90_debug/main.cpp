#define LABSTEST_MAIN
#include "labstest.hpp"
#include "save_bugs.hpp"

using namespace gba;

// Real-hardware expectations; the seeded bugs fail these.
TEST(flash, program_only_clears_bits) {
    u8 cell = 0xFF;
    flash_program_byte(cell, 0x0F);
    EXPECT_EQ(cell, 0x0F);      // cleared to value's bits
    flash_program_byte(cell, 0xF0);
    EXPECT_EQ(cell, 0x00);      // cannot set previously cleared bits
}

TEST(flash, addressing_respects_device_size) {
    // 64 KiB device: bank and high address bits must not escape the window.
    EXPECT_EQ(flash_byte_offset(false, 1, 0x00012345u), 0x2345u);
    EXPECT_EQ(flash_byte_offset(false, 0, 0x00010000u), 0x0000u);
    // 128 KiB device: bank selects the upper half.
    EXPECT_EQ(flash_byte_offset(true, 1, 0x00012345u), 0x12345u);
    EXPECT_EQ(flash_byte_offset(true, 0, 0x00012345u), 0x12345u);
}
