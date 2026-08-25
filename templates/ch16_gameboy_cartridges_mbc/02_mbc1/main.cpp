// Tests for exercise 02: MBC1 bank identity, mode switching, RAM gating.
// ROM images are synthesized in-memory: every byte of physical bank k
// equals k & 0xFF (except the header hole in bank 0), so a single read
// identifies the mapped bank.
#define LABSTEST_MAIN
#include <cstdint>
#include <cstring>
#include <vector>

#include "labstest.hpp"
#include "mbc1.hpp"

namespace {

constexpr size_t kBank = 0x4000;

std::vector<uint8_t> makePatternRom(size_t nbanks) {
    std::vector<uint8_t> rom(nbanks * kBank);
    for (size_t b = 0; b < nbanks; ++b)
        std::memset(rom.data() + b * kBank, static_cast<int>(b & 0xFF),
                    kBank);
    // Header hole in bank 0 so the image could pass as a real cart.
    std::memset(rom.data(), 0x00, 0x150);
    rom[0x147] = 0x01;  // MBC1
    return rom;
}

}  // namespace

TEST(mbc1_regs, ram_enable_window) {
    auto rom = makePatternRom(32);
    cart::Mbc1 m(rom.data(), rom.size(), 0x2000);
    EXPECT_FALSE(m.ramEnabled());
    m.writeReg(0x0000, 0x0A);
    EXPECT_TRUE(m.ramEnabled());
    m.writeReg(0x0000, 0x3B);   // low nibble != A: disables again
    EXPECT_FALSE(m.ramEnabled());
    m.writeReg(0x0000, 0x0A);
    EXPECT_TRUE(m.ramEnabled());
    m.writeReg(0x0000, 0x00);
    EXPECT_FALSE(m.ramEnabled());
}

TEST(mbc1_regs, bank1_five_bits_and_zero_wraps_to_one) {
    auto rom = makePatternRom(32);
    cart::Mbc1 m(rom.data(), rom.size(), 0x2000);
    EXPECT_EQ(m.bank1(), 1);                 // power-up default
    m.writeReg(0x2000, 0x05);
    EXPECT_EQ(m.bank1(), 5);
    m.writeReg(0x2000, 0x00);
    EXPECT_EQ(m.bank1(), 1);                 // 0 -> 1 quirk
    m.writeReg(0x2000, 0xFF);
    EXPECT_EQ(m.bank1(), 0x1F);              // masked to 5 bits, not 6
    EXPECT_EQ(m.physicalBankHi(), size_t{31});
}

TEST(mbc1_banks, hi_half_routes_bank2_and_bank1) {
    auto rom = makePatternRom(128);          // 2 MiB needs all of bank2
    cart::Mbc1 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x4000, 0x03);                // bank2 = 3
    m.writeReg(0x2000, 0x11);                // bank1 = 17
    EXPECT_EQ(m.physicalBankHi(), size_t{(3u << 5) | 17});
    EXPECT_EQ(m.readRom(0x4000), 113);   // pattern byte = bank id (96+17)
    EXPECT_EQ(m.readRom(0x7FFF), 113);
}

TEST(mbc1_banks, out_of_range_selects_wrap_modulo_rom_size) {
    auto rom = makePatternRom(32);
    cart::Mbc1 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x4000, 0x01);                // bank2 = 1 on a 32-bank cart
    m.writeReg(0x2000, 0x01);                // raw select = 33
    EXPECT_EQ(m.physicalBankHi(), size_t{1});  // 33 % 32 wraps
    EXPECT_EQ(m.readRom(0x4001), 0x01);
}

TEST(mbc1_banks, mode0_pins_low_half_to_bank0_mode1_uses_bank2) {
    auto rom = makePatternRom(128);
    cart::Mbc1 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x4000, 0x04);                // bank2 = 4
    // Mode 0: $0000-$3FFF always physical bank 0.
    EXPECT_EQ(m.readRom(0x0200), 0x00);
    m.writeReg(0x6000, 0x01);                // mode 1
    // Mode 1: low half shows bank (bank2<<5) = 128 % 128 = 0... pick 2:
    m.writeReg(0x4000, 0x02);                // bank2 = 2 -> low bank 64
    EXPECT_EQ(m.readRom(0x0200), 64);
    EXPECT_EQ(m.readRom(0x0100), 64);        // even the "header" region
}

TEST(mbc1_ram, enable_gating_absent_ram_and_banking) {
    auto rom = makePatternRom(32);
    cart::Mbc1 absent(rom.data(), rom.size(), 0);
    absent.writeReg(0x0000, 0x0A);
    EXPECT_EQ(absent.readRam(0xA000), 0xFF);  // no chip on the cart

    cart::Mbc1 m(rom.data(), rom.size(), 0x8000);  // 32 KiB = 4 banks
    EXPECT_EQ(m.readRam(0xA000), 0xFF);       // disabled reads open bus
    m.writeRam(0xA000, 0xAA);                 // silent while disabled
    EXPECT_EQ(m.readRam(0xA000), 0xFF);

    m.writeReg(0x0000, 0x0A);
    m.writeRam(0xA123, 0x5A);
    EXPECT_EQ(m.readRam(0xA123), 0x5A);

    // Mode 1 routes bank2 into the RAM bank number.
    m.writeReg(0x6000, 0x01);
    m.writeReg(0x4000, 0x02);                 // RAM bank 2
    m.writeRam(0xA000, 0xC2);
    EXPECT_EQ(m.readRam(0xA000), 0xC2);
    EXPECT_EQ(m.readRam(0xC000), 0x00);       // bank 0 untouched
    m.writeReg(0x6000, 0x00);                 // mode 0 pins RAM bank 0
    EXPECT_EQ(m.readRam(0xA000), 0x00);
}

TEST(mbc1_factory, dispatches_mbc1_family_only) {
    auto romOnly = makePatternRom(2);
    romOnly[0x147] = 0x00;
    EXPECT_EQ(cart::CartridgeController::makeMapper(romOnly.data(),
                                                    romOnly.size()),
              nullptr);
    auto mbc1 = makePatternRom(32);
    mbc1[0x147] = 0x03;
    auto m = cart::CartridgeController::makeMapper(mbc1.data(), mbc1.size());
    EXPECT_NE(m.get(), nullptr);
    EXPECT_EQ(m->readRom(0x4001), 0x01);      // power-up bank 1
}
