// Tests for exercise 04: MBC5 9-bit banking and battery SRAM round-trip.
#define LABSTEST_MAIN
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "labstest.hpp"
#include "mbc5.hpp"

namespace {

constexpr const char* kSavPath = "ch16_sram_test.sav";

std::vector<uint8_t> makePatternRom(size_t nbanks) {
    std::vector<uint8_t> rom(nbanks * 0x4000);
    for (size_t b = 0; b < nbanks; ++b)
        std::memset(rom.data() + b * 0x4000, static_cast<int>(b & 0xFF),
                    0x4000);
    rom[0x147] = 0x1A;  // MBC5+RAM
    return rom;
}

struct SavFileGuard {
    ~SavFileGuard() { std::remove(kSavPath); }
};

}  // namespace

TEST(mbc5_banks, low_eight_bits_select_directly) {
    auto rom = makePatternRom(64);             // 1 MiB
    cart::Mbc5 m(rom.data(), rom.size(), 0x2000);
    EXPECT_EQ(m.romBank9(), 0);                // MBC5 powers up on bank 0
    m.writeReg(0x2000, 0x2A);
    EXPECT_EQ(m.computedBank(0x4000), size_t{42});
    EXPECT_EQ(m.readRom(0x4321), 42);          // pattern byte = bank id
    m.writeReg(0x2000, 0x00);                  // bank 0 IS legal on MBC5
    EXPECT_EQ(m.readRom(0x4000), 0);
}

TEST(mbc5_banks, ninth_bit_extends_the_bank_number) {
    auto rom = makePatternRom(64);
    cart::Mbc5 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x2000, 0xFF);                  // low bits all set
    m.writeReg(0x3000, 0x01);                  // bit 9 set -> select 511
    EXPECT_EQ(m.romBank9(), 511);              // stored full 9-bit value
    EXPECT_EQ(m.computedBank(0x4000), size_t{511 % 64});   // wrapped by HW
    m.writeReg(0x3000, 0x00);                  // bit 9 clears again
    EXPECT_EQ(m.romBank9(), 255);
    m.writeReg(0x3000, 0xFE);                  // only bit 0 is meaningful
    EXPECT_EQ(m.romBank9(), 255);
}

TEST(mbc5_banks, nine_bit_math_survives_any_low_byte) {
    auto rom = makePatternRom(64);
    cart::Mbc5 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x3000, 0x01);                  // bit 9 first...
    m.writeReg(0x2000, 0x05);                  // ...then a plain low write
    EXPECT_EQ(m.romBank9(), 256 + 5);          // must not clobber bit 9
    EXPECT_EQ(m.readRom(0x5000), (256 + 5) % 64);
}

TEST(mbc5_ram, sixteen_banks_no_mode_quirk) {
    auto rom = makePatternRom(64);
    cart::Mbc5 m(rom.data(), rom.size(), 0x80000);   // 512 KiB = 16 banks
    m.writeReg(0x0000, 0x0A);
    m.writeReg(0x4000, 0x0F);
    EXPECT_EQ(m.ramBank(), 15);
    m.writeRam(0xA000, 0x0F);
    EXPECT_EQ(m.readRam(0xA000), 0x0F);
    m.writeReg(0x4000, 0x10 | 0x01);           // upper bits ignored
    EXPECT_EQ(m.ramBank(), 1);
    EXPECT_EQ(m.readRam(0xA000), 0x00);        // different bank
    m.writeReg(0x0000, 0x00);
    EXPECT_EQ(m.readRam(0xA000), 0xFF);        // gate closed
}

TEST(mbc5_battery, sram_round_trip_through_disk) {
    SavFileGuard guard;
    auto rom = makePatternRom(64);
    cart::Mbc5 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x0000, 0x0A);
    m.writeRam(0xA000, 0xDE);
    m.writeRam(0xAFFF, 0xAD);
    EXPECT_TRUE(m.saveSram(kSavPath));

    // A fresh cartridge restores the exact battery contents.
    cart::Mbc5 restored(rom.data(), rom.size(), 0x2000);
    EXPECT_TRUE(restored.loadSram(kSavPath));
    restored.writeReg(0x0000, 0x0A);
    EXPECT_EQ(restored.readRam(0xA000), 0xDE);
    EXPECT_EQ(restored.readRam(0xAFFF), 0xAD);

    // Truncated file: load refuses without corrupting live state.
    {
        FILE* f = std::fopen(kSavPath, "wb");
        EXPECT_TRUE(f != nullptr);
        std::fwrite("xx", 1, 2, f);
        std::fclose(f);
    }
    cart::Mbc5 shortFile(rom.data(), rom.size(), 0x2000);
    EXPECT_FALSE(shortFile.loadSram(kSavPath));
    shortFile.writeReg(0x0000, 0x0A);
    EXPECT_EQ(shortFile.readRam(0xA000), 0x00);

    // Missing file fails cleanly too.
    std::remove(kSavPath);
    EXPECT_FALSE(shortFile.loadSram(kSavPath));
}

TEST(mbc5_factory, dispatches_mbc5_family_only) {
    auto other = makePatternRom(2);
    other[0x147] = 0x01;
    EXPECT_EQ(cart::CartridgeController::makeMapper(other.data(),
                                                    other.size()),
              nullptr);
    auto mbc5 = makePatternRom(64);
    mbc5[0x147] = 0x19;
    auto m = cart::CartridgeController::makeMapper(mbc5.data(), mbc5.size());
    EXPECT_NE(m.get(), nullptr);
}
