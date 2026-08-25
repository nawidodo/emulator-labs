// Tests for exercise 03: MBC3 registers, latch handshake, and the
// deterministic RTC. All time advances are injected cycle counts — no
// wall clock anywhere.
#define LABSTEST_MAIN
#include <cstdint>
#include <cstring>
#include <vector>

#include "labstest.hpp"
#include "mbc3.hpp"

namespace {

constexpr uint64_t kS = cart::kCyclesPerSecond;   // one second in T-cycles

std::vector<uint8_t> makePatternRom(size_t nbanks) {
    std::vector<uint8_t> rom(nbanks * 0x4000);
    for (size_t b = 0; b < nbanks; ++b)
        std::memset(rom.data() + b * 0x4000, static_cast<int>(b & 0xFF),
                    0x4000);
    return rom;
}

}  // namespace

TEST(mbc3_regs, rom_bank_seven_bits_zero_wraps) {
    auto rom = makePatternRom(64);
    cart::Mbc3 m(rom.data(), rom.size(), 0x2000);
    EXPECT_EQ(m.bank1(), 1);
    m.writeReg(0x2000, 0x2A);
    EXPECT_EQ(m.readRom(0x4000), 0x2A);       // pattern byte = bank id
    m.writeReg(0x2000, 0xFF);
    EXPECT_EQ(m.bank1(), 0x7F);               // masked to 7 bits
    EXPECT_EQ(m.readRom(0x4123), 63);    // 127 wraps mod 64 banks
    m.writeReg(0x2000, 0x80 | 0x09);          // upper bits ignored
    EXPECT_EQ(m.bank1(), 9);
}

TEST(mbc3_regs, ram_or_rtc_select_window) {
    auto rom = makePatternRom(64);
    cart::Mbc3 m(rom.data(), rom.size(), 0x8000);
    m.writeReg(0x4000, 0x02);
    EXPECT_EQ(m.ramBankOrRtc(), 0x02);
    m.writeReg(0x4000, 0x0C);                 // RTC select: day high
    EXPECT_EQ(m.ramBankOrRtc(), 0x0C);
    m.writeReg(0x4000, 0x05);                 // neither RAM nor RTC
    EXPECT_EQ(m.ramBankOrRtc(), 0x0C);        // ignored
}

TEST(mbc3_rtc, tick_carry_chain_and_subsecond_drop) {
    cart::Rtc r{};
    r.tick(kS - 1);                            // 59,999,999 cycles < 1 s
    EXPECT_EQ(r.secs, 0);                      // sub-second remainder drops
    r.tick(kS + 1);
    EXPECT_EQ(r.secs, 1);
    r.secs = 59;
    r.tick(kS);
    EXPECT_EQ(r.secs, 0);
    EXPECT_EQ(r.mins, 1);
    r.mins = 59;
    r.hours = 23;
    r.tick(static_cast<uint64_t>(60) * kS);    // one minute -> new hour+day
    EXPECT_EQ(r.hours, 0);
    EXPECT_EQ(r.days, 1);
    EXPECT_EQ(r.daysHi & 0x01, 0);
}

TEST(mbc3_rtc, day_counter_ninth_bit_carries_into_dayshi) {
    cart::Rtc r{};
    r.days = 0xFF;                             // 511 full days next carry
    r.daysHi = 0x01;
    // Advance exactly one day.
    const uint64_t oneDay = static_cast<uint64_t>(24) * 60 * 60 * kS;
    r.tick(oneDay);
    EXPECT_EQ(r.days, 0x00);
    EXPECT_EQ(r.daysHi & 0x01, 0x00);          // 9-bit counter wrapped to 0
}

TEST(mbc3_rtc, halt_bit_freezes_everything) {
    cart::Rtc r{};
    r.daysHi = 0x40;                           // HALT set
    r.tick(100 * kS);
    EXPECT_EQ(r.secs, 0);
    EXPECT_EQ(r.mins, 0);
    EXPECT_EQ(r.hours, 0);
    EXPECT_EQ(r.days, 0);
    r.daysHi = 0x00;                           // resume
    r.tick(kS);
    EXPECT_EQ(r.secs, 1);
}

TEST(mbc3_latch, only_00_then_01_freezes_reads) {
    auto rom = makePatternRom(64);
    cart::Mbc3 m(rom.data(), rom.size(), 0x2000);
    m.tickRtc(65 * kS);                        // live: 1 min 5 s
    EXPECT_FALSE(m.frozen());

    m.writeReg(0x6000, 0x01);                  // wrong order first...
    m.writeReg(0x6000, 0x00);
    EXPECT_FALSE(m.frozen());                  // ...must not latch

    m.writeReg(0x6000, 0x00);                  // proper handshake
    m.writeReg(0x6000, 0x01);
    EXPECT_TRUE(m.frozen());
    EXPECT_EQ(m.latchedRtc().secs, 5);
    EXPECT_EQ(m.latchedRtc().mins, 1);

    // Live clock keeps running after the freeze; reads stay stale.
    m.tickRtc(10 * kS);
    EXPECT_EQ(m.liveRtc().secs, 15);
    m.writeReg(0x4000, 0x08);
    EXPECT_EQ(m.readRam(0xA000), 5);           // shadow seconds
    m.writeReg(0x4000, 0x09);
    EXPECT_EQ(m.readRam(0xA000), 1);           // shadow minutes

    // A fresh latch re-captures the live value.
    m.writeReg(0x6000, 0x00);
    m.writeReg(0x6000, 0x01);
    m.writeReg(0x4000, 0x08);
    EXPECT_EQ(m.readRam(0xA000), 15);
}

TEST(mbc3_ram, rtc_writes_hit_live_and_halt_bit_is_writable) {
    auto rom = makePatternRom(64);
    cart::Mbc3 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x0000, 0x0A);
    m.writeReg(0x4000, 0x01);                  // RAM bank 1
    m.writeRam(0xA010, 0x77);
    EXPECT_EQ(m.readRam(0xA010), 0x77);

    m.writeReg(0x4000, 0x0C);                  // daysHi register
    m.writeRam(0xA000, 0x40);                  // set HALT through the bus
    EXPECT_EQ(m.liveRtc().daysHi & 0x40, 0x40);
    m.tickRtc(60 * kS);
    EXPECT_EQ(m.liveRtc().secs, 0);            // halted: nothing moved
}

TEST(mbc3_ram, disabled_or_absent_ram_reads_open_bus) {
    auto rom = makePatternRom(64);
    cart::Mbc3 absent(rom.data(), rom.size(), 0);
    absent.writeReg(0x0000, 0x0A);
    absent.writeReg(0x4000, 0x01);
    EXPECT_EQ(absent.readRam(0xA000), 0xFF);

    cart::Mbc3 m(rom.data(), rom.size(), 0x2000);
    m.writeReg(0x4000, 0x01);
    EXPECT_EQ(m.readRam(0xA000), 0xFF);        // gate still closed
}
