// Integration tests for the cartridge controller + runner support code.
// Images are synthesized in-memory with bank-id byte patterns.
#define LABSTEST_MAIN
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "labstest.hpp"
#include "cart.hpp"

namespace {

std::vector<uint8_t> makeCart(size_t nbanks, uint8_t type, uint8_t ramCode) {
    std::vector<uint8_t> rom(nbanks * 0x4000, 0x00);
    for (size_t b = 0; b < nbanks; ++b)
        std::memset(rom.data() + b * 0x4000, static_cast<int>(b & 0xFF),
                    0x4000);
    std::memset(rom.data(), 0x00, 0x150);   // header hole in bank 0
    rom[0x147] = type;
    uint8_t sizeCode = 0;
    for (size_t b = 2; b < nbanks; b <<= 1) ++sizeCode;   // banks = 2<<code
    rom[0x148] = sizeCode;
    rom[0x149] = ramCode;
    return rom;
}

}  // namespace

TEST(cart_factory, selects_strategy_from_header_type) {
    const uint8_t types[] = {0x00, 0x03, 0x0F, 0x19, 0xBE};
    for (uint8_t t : types) {
        auto rom = makeCart(2, t, 0x02);
        auto m = cart::CartridgeController::makeMapper(rom.data(),
                                                       rom.size());
        EXPECT_TRUE(m != nullptr);
    }
}

TEST(cart_mbc1, end_to_end_bank_identity_through_factory) {
    auto rom = makeCart(32, 0x03, 0x02);      // 512 KiB MBC1+RAM
    auto m = cart::CartridgeController::makeMapper(rom.data(), rom.size());
    EXPECT_TRUE(m.get() != nullptr);
    EXPECT_EQ(m->readRom(0x4000), 0x01);      // power-up: physical bank 1
    m->writeReg(0x2000, 0x07);
    EXPECT_EQ(m->readRom(0x4321), 0x07);
    m->writeReg(0x0000, 0x0A);
    EXPECT_EQ(m->readRam(0xA000), 0x00);      // enabled: powered-up SRAM
    m->writeReg(0x0000, 0x00);
    EXPECT_EQ(m->readRam(0xA000), 0xFF);      // disabled: open bus
}

TEST(cart_mbc3, rtc_tick_and_latch_via_bus_writes) {
    auto rom = makeCart(64, 0x0F, 0x02);
    auto m = cart::CartridgeController::makeMapper(rom.data(), rom.size());
    EXPECT_TRUE(m.get() != nullptr);
    m->tickRtc(65 * cart::kCyclesPerSecond);  // 1 min 5 s of injected time
    m->writeReg(0x4000, 0x08);                // RTC seconds register
    EXPECT_EQ(m->readRam(0xA000), 5);         // live read before latching
    m->writeReg(0x6000, 0x00);
    m->writeReg(0x6000, 0x01);                // latch handshake
    m->tickRtc(10 * cart::kCyclesPerSecond);
    EXPECT_EQ(m->readRam(0xA000), 5);         // frozen shadow value
}

TEST(cart_mbc5, nine_bit_select_wraps_into_small_image) {
    auto rom = makeCart(64, 0x19, 0x02);      // 1 MiB image
    auto m = cart::CartridgeController::makeMapper(rom.data(), rom.size());
    EXPECT_TRUE(m.get() != nullptr);
    m->writeReg(0x3000, 0x01);                // bit 9
    m->writeReg(0x2000, 0x05);                // select 261
    EXPECT_EQ(m->readRom(0x4000),
              static_cast<uint8_t>(261 % 64));  // hardware wrap
}

TEST(cart_mbcx, spec_examples_over_a_real_image) {
    auto rom = makeCart(2, 0xBE, 0x00);       // 32 KiB: exactly two banks
    std::memset(rom.data() + 0x4000, 0x2A, 0x4000);   // bank 1 marker
    auto m = cart::CartridgeController::makeMapper(rom.data(), rom.size());
    EXPECT_TRUE(m.get() != nullptr);
    EXPECT_EQ(m->readRom(0x4000), 0x2A);      // default R1 = 1
    m->writeReg(0x2000, 0x09);                // masked to 3 bits -> 1
    EXPECT_EQ(m->readRom(0x4000), 0x2A);
    m->writeReg(0x4000, 0x01);                // soft open bus
    EXPECT_EQ(m->readRom(0x4000), 0xFF);
    m->writeReg(0x4000, 0x00);
    EXPECT_EQ(m->readRom(0x4000), 0x2A);
}
