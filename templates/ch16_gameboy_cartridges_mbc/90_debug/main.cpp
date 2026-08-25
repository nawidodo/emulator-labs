// Tests for 90_debug: each suite targets exactly one seeded mapper
// defect. All three must be fixed before the suite goes green.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "cart_debug.hpp"

TEST(debug_mbc1, bank1_is_five_bits_not_six) {
    cartdbg::DebugMbc1 m{nullptr, 64};   // pretend a 1 MiB cart
    m.writeBank1(0x21);                  // game wrote $21 (bank 1 + bit 5)
    EXPECT_EQ(m.bank1, 0x01);            // masked to five bits...
    EXPECT_EQ(m.physicalBankHi(), size_t{1});
    m.writeBank1(0x3F);                  // max legal select
    EXPECT_EQ(m.bank1, 0x1F);
    EXPECT_EQ(m.physicalBankHi(), size_t{31});
}

TEST(debug_mbc3, latch_handshake_is_00_then_01) {
    cartdbg::DebugMbc3 m;
    m.liveSecs = 42;
    m.latchClock(0x01);                  // inverted order first...
    m.latchClock(0x00);
    EXPECT_FALSE(m.frozen);            // ...must NOT latch
    m.latchClock(0x00);                  // documented handshake
    m.latchClock(0x01);
    EXPECT_TRUE(m.frozen);
    m.liveSecs = 99;                     // clock keeps running after freeze
    EXPECT_EQ(m.readSeconds(), 42);      // reads come from the shadow
}

TEST(debug_mbc5, high_bank_window_sets_bit_nine) {
    cartdbg::DebugMbc5 m{512};           // 8 MiB cart: 512 banks
    m.writeRegHigh(0x01);
    m.writeRegLow(0x05);                 // select = 256 + 5
    EXPECT_EQ(m.romBank, 261);
    EXPECT_EQ(m.computedBank(), size_t{261});
    m.writeRegHigh(0x00);                // bit 9 clears again
    EXPECT_EQ(m.computedBank(), size_t{5});
}
