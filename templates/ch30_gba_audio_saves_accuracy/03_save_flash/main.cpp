#define LABSTEST_MAIN
#include "labstest.hpp"
#include "save.hpp"

using namespace gba;

namespace {

// Issue the AA 55 op prefix.
void cmd(FlashChip& f, u8 op) {
    f.write(0x0E000000, 0xAA);
    f.write(0x0E000002, 0x55);
    f.write(0x0E000000, op);
}

}  // namespace

TEST(flash, id_mode_enter_exit) {
    FlashChip f(true);
    cmd(f, 0x90);
    EXPECT_EQ(f.read(0x0E000000), kFlashMfgId);
    EXPECT_EQ(f.read(0x0E000001), kFlashDevId128K);
    cmd(f, 0xF0);
    EXPECT_EQ(f.read(0x0E000000), 0xFF);  // erased memory again
}

TEST(flash, id_needs_exact_prefix) {
    FlashChip f(false);
    // Wrong first byte: nothing happens.
    f.write(0x0E000000, 0xAB);
    f.write(0x0E000002, 0x55);
    f.write(0x0E000000, 0x90);
    EXPECT_NE(f.read(0x0E000000), kFlashMfgId);
    // Incomplete prefix does not latch either.
    f.write(0x0E000000, 0xAA);
    f.write(0x0E000002, 0x90);
    EXPECT_NE(f.read(0x0E000001), kFlashDevId64K);
}

TEST(flash, chip_and_sector_erase) {
    FlashChip f(false);
    // Program a couple of bytes via A0.
    cmd(f, 0xA0);
    f.write(0x10, 0x12);
    cmd(f, 0xA0);
    f.write(0x11, 0x34);
    EXPECT_EQ(f.read(0x10), 0x12);   // programming onto erased FF keeps bits
    // Chip erase restores FF everywhere.
    cmd(f, 0x80);
    cmd(f, 0x10);
    EXPECT_EQ(f.read(0x10), 0xFF);

    // Sector erase only touches the 4 KiB page.
    cmd(f, 0xA0);
    f.write(0x2000, 0xF0);
    cmd(f, 0xA0);
    f.write(0x2FFE, 0x0F);
    // Arm erase, then confirm with AA 55 30 AT the sector address.
    cmd(f, 0x80);
    f.write(0x0E002000, 0xAA);
    f.write(0x0E002002, 0x55);
    f.write(0x0E002000, 0x30);
    EXPECT_EQ(f.read(0x2000), 0xFF);
    EXPECT_EQ(f.read(0x2FFF), 0xFF);
    EXPECT_EQ(f.read(0x1000 + 4), 0xFF);
}

TEST(flash, program_ands_bits_only_clear) {
    FlashChip f(false);
    cmd(f, 0xA0);
    f.write(0x40, 0b1010'1010);
    cmd(f, 0xA0);
    f.write(0x40, 0b1100'1100);      // attempt to set cleared bits: ignored
    EXPECT_EQ(f.read(0x40), u8(0b1000'1000));
}

TEST(flash, bank_switching_128k_only) {
    FlashChip f(true);
    cmd(f, 0xB0);
    f.write(0x0E000000, 1);
    cmd(f, 0xA0);
    f.write(0x20, 0x5A);             // program in bank 1
    EXPECT_EQ(f.read(0x20), 0x5A);   // still bank 1 view
    cmd(f, 0xB0);
    f.write(0x0E000000, 0);
    EXPECT_EQ(f.read(0x20), 0xFF);   // bank 0 untouched

    FlashChip small(false);
    cmd(small, 0xB0);
    small.write(0x0E000000, 1);      // ignored on 64K device
    cmd(small, 0xA0);
    small.write(0x20, 0x77);
    EXPECT_EQ(small.read(0x20), 0x77);
}

TEST(sram, byte_array_semantics) {
    Sram s;
    s.write(0x1234, 0x9C);
    EXPECT_EQ(s.read(0x1234), 0x9C);
    // Unlike flash: plain overwrite (no AND semantics).
    s.write(0x1234, 0x00);
    EXPECT_EQ(s.read(0x1234), 0x00);
}
