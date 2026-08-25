#define LABSTEST_MAIN
#include "labstest.hpp"

#include <string>
#include <vector>

#include "ines.hpp"
#include "mfx.hpp"

using nes23cart::Cart;
using nes23mfx::Mfx1;

namespace {

// 4 PRG banks (byte 0xB0+bank), 4 CHR units of 2 KiB (0x30+unit).
Cart cart() {
    Cart c;
    c.mapper = 0x99;  // MFX-1's fictional number
    for (int b = 0; b < 4; ++b)
        c.prg.insert(c.prg.end(), 16384, uint8_t(0xB0 + b));
    for (int u = 0; u < 4; ++u) c.chr.insert(c.chr.end(), 2048, uint8_t(0x30 + u));
    return c;
}

}  // namespace

TEST(nes23mfx, register_file_lives_at_8000_by_low_addr_bits) {
    Mfx1 m(cart());
    m.cpu_write(0x8002, 0xAB);  // addr bits 1-0 == 2 -> R2
    m.cpu_write(0xBFFF, 0xCD);  // any address in range, bits 1-0 == 3 -> R3
    EXPECT_EQ(m.debug_snapshot(), "mfx r0=00 r1=00 r2=ab r3=cd wc=2 irq=0");
}

TEST(nes23mfx, prg_low_bank_and_fixed_last_bank_mode_a) {
    Mfx1 m(cart());
    m.cpu_write(0x8000, 2);     // R0 = bank 2 -> $8000-$BFFF
    EXPECT_EQ(m.cpu_read(0x8000), 0xB2);
    EXPECT_EQ(m.cpu_read(0xC000), 0xB3);  // mode A: last bank pinned
}

TEST(nes23mfx, prg_mode_b_mirrors_r0_into_the_high_half) {
    Mfx1 m(cart());
    m.cpu_write(0x8001, 0x01);  // R1 bit 0: mirror mode
    m.cpu_write(0x8000, 1);
    EXPECT_EQ(m.cpu_read(0x8000), 0xB1);
    EXPECT_EQ(m.cpu_read(0xFFFF), 0xB1);
}

TEST(nes23mfx, echo_chr_replicates_one_unit_four_times) {
    Mfx1 m(cart());
    m.cpu_write(0x8002, 1);     // R2 = CHR unit 1
    for (uint16_t a : {0x0000, 0x07FF, 0x0800, 0x17FF})
        EXPECT_EQ(m.ppu_read(a), 0x31);
    // PPU writes to CHR ROM are dropped.
    m.ppu_write(0x0005, 0xEE);
    EXPECT_EQ(m.ppu_read(0x0005), 0x31);
}

TEST(nes23mfx, one_shot_timer_ticks_every_fourth_register_write) {
    Mfx1 m(cart());
    m.cpu_write(0x8003, 2);     // arm with period 2 (write #1; also acks)
    m.cpu_write(0x8000, 0);     // write #2
    m.cpu_write(0x8001, 0);     // write #3: still no tick
    EXPECT_EQ(m.timer_count(), 2);
    EXPECT_FALSE(m.irq_line());
    m.cpu_write(0x8002, 0);     // write #4 -> tick, count 2->1
    EXPECT_EQ(m.timer_count(), 1);
    m.cpu_write(0x8000, 0);     // writes #5-#7: no tick
    m.cpu_write(0x8001, 0);
    m.cpu_write(0x8002, 0);
    EXPECT_EQ(m.timer_count(), 1);
    EXPECT_FALSE(m.irq_line());
    m.cpu_write(0x803D, 0);     // write #8 (bits 1-0 = 1) -> tick, fire!
    EXPECT_TRUE(m.irq_line());
    // One-shot: disarmed on firing, so further writes never re-tick.
    for (int i = 0; i < 8; ++i) m.cpu_write(0x8000, 0);
    EXPECT_TRUE(m.irq_line());
}

TEST(nes23mfx, rearming_acks_and_restarts_the_countdown) {
    Mfx1 m(cart());
    m.cpu_write(0x8003, 1);     // arm period 1
    m.cpu_write(0x8000, 0);
    m.cpu_write(0x8001, 0);
    m.cpu_write(0x8002, 0);     // write #4 -> tick -> fires
    EXPECT_TRUE(m.irq_line());
    m.cpu_write(0x8003, 5);     // fresh R3 write: acknowledges AND rearms
    EXPECT_FALSE(m.irq_line());
}

TEST(nes23mfx, prg_ram_at_6000_survives_banking) {
    Mfx1 m(cart());
    m.cpu_write(0x6ABC, 0x42);
    EXPECT_EQ(m.cpu_read(0x6ABC), 0x42);
}
