#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "mmc3.hpp"

using nes23map::Cart;
using nes23mmc3::Mmc3;

namespace {

// PRG: eight 8 KiB banks, bank b filled with byte 0x80+b.
// CHR: sixteen 1 KiB units, unit u filled with byte 0x10+u.
Cart cart() {
    Cart c;
    for (int b = 0; b < 8; ++b)
        c.prg.insert(c.prg.end(), 8192, uint8_t(0x80 + b));
    for (int u = 0; u < 16; ++u)
        c.chr.insert(c.chr.end(), 1024, uint8_t(0x10 + u));
    return c;
}

void select(Mmc3& m, uint8_t cmd, uint8_t data) {
    m.cpu_write(0x8000, cmd);
    m.cpu_write(0x8001, data);
}

}  // namespace

TEST(nes23mmc3, register_decode_uses_address_parity_not_decoding_magic) {
    Mmc3 m(cart());
    select(m, 6, 2);  // R6 = PRG bank 2
    EXPECT_EQ(m.cpu_read(0x8000), 0x82);
    EXPECT_EQ(m.cpu_read(0x9FFF), 0x82);   // whole 8 KiB window
    EXPECT_EQ(m.cpu_read(0xA000), 0x87);   // $A000 follows R7 (default 7)
}

TEST(nes23mmc3, fixed_prg_tail_is_second_to_last_and_last) {
    Mmc3 m(cart());
    EXPECT_EQ(m.cpu_read(0xC000), 0x86);   // second-to-last bank (6)
    EXPECT_EQ(m.cpu_read(0xE000), 0x87);   // last bank (7)
    select(m, 6, 1);                       // moving R6 must NOT move $C000
    EXPECT_EQ(m.cpu_read(0xC000), 0x86);
    EXPECT_EQ(m.cpu_read(0x8000), 0x81);
}

TEST(nes23mmc3, prg_inversion_swaps_r6_window_with_the_fixed_slot) {
    Mmc3 m(cart());
    select(m, 0x20 | 6, 1);  // bit 5: PRG inversion; R6 = bank 1
    EXPECT_EQ(m.cpu_read(0xC000), 0x81);   // R6 now lives high
    EXPECT_EQ(m.cpu_read(0x8000), 0x86);   // fixed bank moved low
}

TEST(nes23mmc3, mirroring_register_polarity_is_inverted_vs_ines) {
    Mmc3 m(cart());
    m.cpu_write(0xA000, 0);  // MMC3: 0 means VERTICAL
    EXPECT_FALSE(m.mirroring_horizontal());
    m.cpu_write(0xA001, 1);  // any address in range works; 1 = horizontal
    EXPECT_TRUE(m.mirroring_horizontal());
}

TEST(nes23mmc3, chr_r0_r1_are_2k_and_move_by_inversion_flag) {
    Mmc3 m(cart());
    select(m, 0, 3);  // R0 = 2K bank 3 -> units 6,7 at $0000-$07FF
    EXPECT_EQ(m.ppu_read(0x0000), 0x16);
    EXPECT_EQ(m.ppu_read(0x0400), 0x17);
    select(m, 0x40 | 0, 3);  // now invert: R0 shows up at $1000-$17FF
    EXPECT_EQ(m.ppu_read(0x1000), 0x16);
    EXPECT_EQ(m.ppu_read(0x0000), 0x12);   // R2 default (2) fills low half
}

TEST(nes23mmc3, chr_r2_through_r5_are_1k_banks) {
    Mmc3 m(cart());
    select(m, 2, 9);
    EXPECT_EQ(m.ppu_read(0x1000), 0x19);   // non-inverted: R2 at $1000
    select(m, 3, 12);
    EXPECT_EQ(m.ppu_read(0x1400), 0x1C);
}

TEST(nes23mmc3, irq_fires_every_latch_plus_one_edges_after_reload) {
    Mmc3 m(cart());
    m.cpu_write(0xC000, 3);   // latch = 3
    m.cpu_write(0xE001, 0);   // enable
    m.cpu_write(0xC001, 0);   // reload request
    for (int edge = 1; edge <= 4; ++edge) {
        m.a12_edge();
        EXPECT_FALSE(m.irq_line());  // counts down 3,2,1,0
    }
    EXPECT_EQ(m.counter(), 0);
    m.a12_edge();                    // fifth edge: reload onto zero -> IRQ
    EXPECT_TRUE(m.irq_line());
    EXPECT_EQ(m.counter(), 3);
    m.a12_edge();                    // assertion is LEVEL-held until acked
    EXPECT_TRUE(m.irq_line());
    m.cpu_write(0xE000, 0);          // disable + acknowledge
    EXPECT_FALSE(m.irq_line());
}

TEST(nes23mmc3, irq_stays_silent_while_disabled_but_latch_survives) {
    Mmc3 m(cart());
    m.cpu_write(0xC000, 1);
    m.cpu_write(0xC001, 0);          // reload while disabled
    for (int i = 0; i < 4; ++i) m.a12_edge();
    EXPECT_FALSE(m.irq_line());      // enabled never set -> no assertion
    // Edges: reload->1, dec->0, reload->1, dec->0.
    EXPECT_EQ(m.counter(), 0);
}

TEST(nes23mmc3, rewriting_the_latch_mid_count_does_not_truncate_the_period) {
    Mmc3 m(cart());
    m.cpu_write(0xE001, 0);          // enable
    m.cpu_write(0xC000, 5);
    m.cpu_write(0xC001, 0);          // reload on next edge
    m.a12_edge();                    // counter = 5
    m.a12_edge();                    // counter = 4
    m.cpu_write(0xC000, 1);          // new period arrives MID-COUNT...
    m.a12_edge();                    // ...but this countdown keeps running
    EXPECT_EQ(m.counter(), 3);
    m.a12_edge();
    EXPECT_EQ(m.counter(), 2);
    m.a12_edge();
    EXPECT_EQ(m.counter(), 1);
    m.a12_edge();
    EXPECT_EQ(m.counter(), 0);
    m.a12_edge();                    // only NOW does the short period land
    EXPECT_TRUE(m.irq_line());
    EXPECT_EQ(m.counter(), 1);
}
