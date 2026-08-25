#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "mmc1.hpp"

using nes23map::Cart;
using nes23mmc1::Mmc1;
using nes23mmc1::Mirroring;

namespace {

// 4 PRG banks: byte at bank*16K+k = 0xB0+bank. CHR: eight 4 KiB units,
// unit u filled with 0x40+u (pattern tables $0000-$1FFF).
Cart cart() {
    Cart c;
    for (int bank = 0; bank < 4; ++bank)
        c.prg.insert(c.prg.end(), 16384, uint8_t(0xB0 + bank));
    for (int unit = 0; unit < 8; ++unit)
        c.chr.insert(c.chr.end(), 4096, uint8_t(0x40 + unit));
    return c;
}

// Feed one MMC1 register load: five LSB-first writes into the register's
// own address window.
void load_reg(Mmc1& m, int reg_index, uint8_t value) {
    for (int bit = 0; bit < 5; ++bit)
        m.cpu_write(uint16_t(0x8000 + reg_index * 0x2000),
                    uint8_t((value >> bit) & 1));
}

TEST(nes23mmc1, shift_register_accumulates_lsb_first) {
    Mmc1 m(cart());
    m.cpu_write(0x8000, 0x80);  // reset
    EXPECT_EQ(m.pending_bits(), 0);
    m.cpu_write(0x8000, 1);
    m.cpu_write(0x8000, 0);
    m.cpu_write(0x8000, 1);
    // Newest bit lands at bit 4; the first-written bit migrates down toward
    // bit 0, arriving on the fifth write (see dispatch test below).
    EXPECT_EQ(m.latch_value(), 0b10100);
    EXPECT_EQ(m.pending_bits(), 3);
}

TEST(nes23mmc1, fifth_write_dispatches_and_clears) {
    Mmc1 m(cart());
    m.cpu_write(0x8000, 0x80);
    for (int i = 0; i < 4; ++i) m.cpu_write(0x8000, 0);
    EXPECT_EQ(m.pending_bits(), 4);  // not dispatched yet
    m.cpu_write(0x8000, 0);          // value 00000 -> register 0 (control)
    EXPECT_EQ(m.pending_bits(), 0);
}

TEST(nes23mmc1, msb_write_resets_and_forces_prg_fix_last) {
    Mmc1 m(cart());
    // Set PRG mode 2 ("fix FIRST") via control writes...
    load_reg(m, 0, 0x08);  // PRG mode bits = 10
    EXPECT_EQ((m.control() >> 2) & 3, 2);
    // ...then a single MSB write snaps mode back to "fix LAST" (bits 11).
    m.cpu_write(0xFFFF, 0x80);
    EXPECT_EQ((m.control() >> 2) & 3, 3);
    EXPECT_EQ(m.pending_bits(), 0);
}

TEST(nes23mmc1, control_mirroring_encoding_is_not_the_ines_one) {
    Mmc1 m(cart());
    load_reg(m, 0, 0x02);  // mirroring field 2 == VERTICAL on MMC1
    EXPECT_TRUE(m.mirroring() == Mirroring::Vertical);
    load_reg(m, 0, 0x03);  // 3 == horizontal
    EXPECT_TRUE(m.mirroring() == Mirroring::Horizontal);
    load_reg(m, 0, 0x00);  // 0 == one-screen, lower bank
    EXPECT_TRUE(m.mirroring() == Mirroring::OneScreenLower);
}

TEST(nes23mmc1, prg_mode3_switches_low_half_only) {
    Mmc1 m(cart());
    load_reg(m, 3, 1);  // PRG reg = bank 1
    EXPECT_EQ(m.cpu_read(0x8000), 0xB1);
    EXPECT_EQ(m.cpu_read(0xBFFF), 0xB1);
    EXPECT_EQ(m.cpu_read(0xC000), 0xB3);  // last bank pinned high
    EXPECT_EQ(m.cpu_read(0xFFFF), 0xB3);
}

TEST(nes23mmc1, prg_mode2_fixes_first_bank_instead) {
    Mmc1 m(cart());
    load_reg(m, 0, 0x08);  // PRG mode 2
    load_reg(m, 3, 2);
    EXPECT_EQ(m.cpu_read(0x8000), 0xB0);  // fixed FIRST bank
    EXPECT_EQ(m.cpu_read(0xC000), 0xB2);  // switchable high half
}

TEST(nes23mmc1, prg_mode0_maps_a_32k_window_ignoring_bit0) {
    Mmc1 m(cart());
    load_reg(m, 0, 0x04);  // PRG mode 0/1 -> 32 KiB window
    load_reg(m, 3, 3);     // bit0 ignored -> banks 2 and 3
    EXPECT_EQ(m.cpu_read(0x8000), 0xB2);
    EXPECT_EQ(m.cpu_read(0xC000), 0xB3);
    load_reg(m, 3, 2);     // same 32K window, bit 0 differs
    EXPECT_EQ(m.cpu_read(0x8000), 0xB2);
}

TEST(nes23mmc1, chr_8k_mode_pairs_even_units_of_chr0) {
    Mmc1 m(cart());
    load_reg(m, 1, 5);  // 8K mode default (bit4=0): units 4|5
    EXPECT_EQ(m.ppu_read(0x0000), 0x44);
    EXPECT_EQ(m.ppu_read(0x1000), 0x45);
}

TEST(nes23mmc1, chr_4k_mode_banks_each_half_independently) {
    Mmc1 m(cart());
    load_reg(m, 0, 0x10);  // CHR mode 1: two independent 4K banks
    load_reg(m, 1, 6);
    load_reg(m, 2, 1);
    EXPECT_EQ(m.ppu_read(0x0000), 0x46);
    EXPECT_EQ(m.ppu_read(0x0FFF), 0x46);
    EXPECT_EQ(m.ppu_read(0x1000), 0x41);
}

TEST(nes23mmc1, prg_ram_sits_at_6000) {
    Mmc1 m(cart());
    m.cpu_write(0x6007, 0x5A);
    EXPECT_EQ(m.cpu_read(0x6007), 0x5A);
}
}
