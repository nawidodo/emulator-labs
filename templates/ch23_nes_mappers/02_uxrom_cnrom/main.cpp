#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "uxrom_cnrom.hpp"

using nes23map::Cart;
using nes23uxcn::CnRom;
using nes23uxcn::UxRom;

namespace {

// Four 16 KiB PRG banks; byte at offset bank*16K + k is 0xB0+bank.
Cart ux_cart() {
    Cart c;
    for (int bank = 0; bank < 4; ++bank)
        c.prg.insert(c.prg.end(), 16384, uint8_t(0xB0 + bank));
    return c;  // chr empty -> CHR RAM board
}

// One 16 KiB PRG bank (mirrored) + four 8 KiB CHR banks.
Cart cn_cart() {
    Cart c;
    c.prg.assign(16384, 0xC1);
    for (int unit = 0; unit < 4; ++unit)
        c.chr.insert(c.chr.end(), 8192, uint8_t(0x30 + unit));
    return c;
}

}  // namespace

TEST(nes23uxcn, uxrom_low_half_switches_via_any_prg_write) {
    UxRom m(ux_cart());
    m.cpu_write(0x8001, 2);  // register decode ignores the exact address
    EXPECT_EQ(m.bank(), 2);
    EXPECT_EQ(m.cpu_read(0x8000), 0xB2);
    m.cpu_write(0xFFFF, 1);  // ANY address in the window works
    EXPECT_EQ(m.cpu_read(0x8000), 0xB1);
}

TEST(nes23uxcn, uxrom_high_half_is_fixed_last_bank) {
    UxRom m(ux_cart());
    for (uint8_t b = 0; b < 4; ++b) {
        m.cpu_write(0x8000, b);
        // $C000-$FFFF never moves, no matter the latch.
        EXPECT_EQ(m.cpu_read(0xC000), 0xB3);
        EXPECT_EQ(m.cpu_read(0xFFFF), 0xB3);
        // ...and the low half follows the latch.
        EXPECT_EQ(m.cpu_read(0x8005), uint8_t(0xB0 + b));
    }
}

TEST(nes23uxcn, uxrom_bank_value_is_masked_to_installed_banks) {
    UxRom m(ux_cart());  // 4 banks -> mask 0b11
    m.cpu_write(0x8000, 6);  // 6 & 3 == 2
    EXPECT_EQ(m.bank(), 2);
    EXPECT_EQ(m.cpu_read(0x8000), 0xB2);
}

TEST(nes23uxcn, uxrom_chr_ram_is_writable_through_the_ppu_side) {
    UxRom m(ux_cart());
    m.ppu_write(0x0000, 0x11);
    m.ppu_write(0x1FFF, 0x22);
    m.ppu_write(0x2005, 0x33);  // wraps into the 8 KiB window
    EXPECT_EQ(m.ppu_read(0x0000), 0x11);
    EXPECT_EQ(m.ppu_read(0x1FFF), 0x22);
    EXPECT_EQ(m.ppu_read(0x0005), 0x33);
}

TEST(nes23uxcn, cnrom_prg_never_switches_and_mirrors_16k_images) {
    CnRom m(cn_cart());
    m.cpu_write(0x8000, 3);  // selects a CHR bank, NOT a PRG bank
    EXPECT_EQ(m.cpu_read(0x8000), 0xC1);   // low half: bank 0 forever
    EXPECT_EQ(m.cpu_read(0xC000), 0xC1);   // high half mirrors (16K image)
}

TEST(nes23uxcn, cnrom_cpu_write_selects_masked_chr_bank) {
    CnRom m(cn_cart());  // 4 x 8 KiB units -> mask 0b11
    m.cpu_write(0xE000, 2);
    EXPECT_EQ(m.bank(), size_t(2));
    EXPECT_EQ(m.ppu_read(0x0000), 0x32);
    m.cpu_write(0x8001, 7);  // 7 & 3 == 3
    EXPECT_EQ(m.ppu_read(0x1000), 0x33);
}

TEST(nes23uxcn, cnrom_ppu_cannot_rewrite_chr_rom) {
    CnRom m(cn_cart());
    m.ppu_write(0x0004, 0xEE);  // CHR ROM: discarded
    EXPECT_EQ(m.ppu_read(0x0004), 0x30);
}
