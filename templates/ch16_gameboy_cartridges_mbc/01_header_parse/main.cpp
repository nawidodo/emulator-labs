// Tests for exercise 01: cartridge header decoding. All fixtures are
// synthesized in-memory (no committed ROM needed) with the same header
// conventions as tests/public/ch16_gameboy_cartridges_mbc/tools/make_roms.py.
#define LABSTEST_MAIN
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "labstest.hpp"
#include "header.hpp"

namespace {

// Build a minimal 32 KiB image with a well-formed header.
std::vector<uint8_t> makeRom(uint8_t type, uint8_t romCode, uint8_t ramCode,
                             const char* titleText) {
    std::vector<uint8_t> rom(0x8000, 0x00);
    rom[0x100] = 0x00;              // nop entry point (synthetic)
    std::strncpy(reinterpret_cast<char*>(&rom[cart::kTitleAddr]), titleText,
                 cart::kTitleLen);
    rom[cart::kTypeAddr] = type;
    rom[cart::kRomSizeAddr] = romCode;
    rom[cart::kRamSizeAddr] = ramCode;
    unsigned sum = 25;
    for (uint16_t a = cart::kTitleAddr; a < cart::kChecksumAddr; ++a)
        sum += rom[a];
    rom[cart::kChecksumAddr] = static_cast<uint8_t>(-(sum & 0xFF));
    return rom;
}

}  // namespace

TEST(header_title, raw_sixteen_bytes_no_trimming) {
    auto rom = makeRom(0x00, 0x00, 0x00, "CH16TUTORIAL");
    // Title shorter than 16 bytes is space/$00 padded by real tools and
    // kept verbatim here: 4 pad bytes of $00 after "CH16TUTORIAL".
    const std::string want = std::string("CH16TUTORIAL") + std::string(4, '\0');
    EXPECT_EQ(cart::title(rom.data()), want);
}

TEST(header_battery, battery_type_table) {
    EXPECT_TRUE(cart::hasBattery(0x03));   // MBC1+RAM+BATTERY
    EXPECT_TRUE(cart::hasBattery(0x06));   // MBC2+BATTERY
    EXPECT_TRUE(cart::hasBattery(0x09));
    EXPECT_TRUE(cart::hasBattery(0x0D));
    EXPECT_TRUE(cart::hasBattery(0x0F));   // MBC3+TIMER+BATTERY
    EXPECT_TRUE(cart::hasBattery(0x10));
    EXPECT_TRUE(cart::hasBattery(0x13));
    EXPECT_TRUE(cart::hasBattery(0x1B));
    EXPECT_TRUE(cart::hasBattery(0x1E));
    EXPECT_FALSE(cart::hasBattery(0x00));  // ROM_ONLY
    EXPECT_FALSE(cart::hasBattery(0x01));  // plain MBC1
    EXPECT_FALSE(cart::hasBattery(0x19));  // plain MBC5
}

TEST(header_controller, names_for_common_mappers) {
    EXPECT_EQ(std::string(cart::controllerName(0x00)), "ROM_ONLY");
    EXPECT_EQ(std::string(cart::controllerName(0x01)), "MBC1");
    EXPECT_EQ(std::string(cart::controllerName(0x03)), "MBC1+RAM+BATTERY");
    EXPECT_EQ(std::string(cart::controllerName(0x0F)),
              "MBC3+TIMER+BATTERY");
    EXPECT_EQ(std::string(cart::controllerName(0x13)),
              "MBC3+RAM+BATTERY");
    EXPECT_EQ(std::string(cart::controllerName(0x1C)), "MBC5+RUMBLE");
    EXPECT_EQ(std::string(cart::controllerName(0xBE)), "UNKNOWN");
}

TEST(header_romsize, powers_of_two_and_special_codes) {
    EXPECT_EQ(cart::romSizeBytes(0x00), size_t{32768});
    EXPECT_EQ(cart::romSizeBytes(0x01), size_t{65536});
    EXPECT_EQ(cart::romSizeBytes(0x08), size_t{8388608});   // 8 MiB
    // Late-production oddballs (documented in SPEC.md):
    EXPECT_EQ(cart::romSizeBytes(0x52), size_t{1048576});   // 1 MiB
    EXPECT_EQ(cart::romSizeBytes(0x53), size_t{1179648});   // 1152 KiB
    EXPECT_EQ(cart::romSizeBytes(0x54), size_t{1310720});   // 1280 KiB
}

TEST(header_ramsize, code_table) {
    EXPECT_EQ(cart::ramSizeBytes(0x00), size_t{0});
    EXPECT_EQ(cart::ramSizeBytes(0x01), size_t{2048});
    EXPECT_EQ(cart::ramSizeBytes(0x02), size_t{8192});
    EXPECT_EQ(cart::ramSizeBytes(0x03), size_t{32768});
    EXPECT_EQ(cart::ramSizeBytes(0x04), size_t{131072});
    EXPECT_EQ(cart::ramSizeBytes(0x05), size_t{65536});
}

TEST(header_checksum, valid_fixture_and_single_byte_corruption) {
    auto rom = makeRom(0x03, 0x08, 0x02, "CH16MBC1");
    EXPECT_TRUE(cart::headerChecksumValid(rom.data()));
    // Corrupting any covered byte (title, type, sizes) breaks it...
    for (uint16_t a : {uint16_t{0x134}, uint16_t{0x147}, uint16_t{0x148}}) {
        auto bad = rom;
        bad[a] ^= 0x01;
        EXPECT_FALSE(cart::headerChecksumValid(bad.data()));
    }
    // ...and so does corrupting the stored checksum itself.
    auto badCk = rom;
    badCk[cart::kChecksumAddr] ^= 0xFF;
    EXPECT_FALSE(cart::headerChecksumValid(badCk.data()));
}
