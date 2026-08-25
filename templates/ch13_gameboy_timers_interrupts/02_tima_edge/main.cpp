// Exercise 13.02 tests -- TAC gate, bit-select table, falling edges.
// Every case drives the divider with crafted tick sequences so the tapped
// bit falls exactly where the comment says it does.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "timer_dev.hpp"

namespace {

// Selected-bit period in T-cycles for each TAC select value (the exact
// table from LECTURE.md).
constexpr int kPeriod[4] = {1024, 16, 64, 256};

}  // namespace

TEST(edge, select00_taps_div_bit9) {
    gb::TimerDevice t;
    t.write_tac(0x04);  // enabled, select 00 -> bit 9
    EXPECT_EQ(gb::TimerDevice::select_shift(t.tac), 9);
    // One full bit-9 period produces exactly one falling edge.
    const uint8_t before = t.tima;
    t.step(kPeriod[0]);
    EXPECT_EQ(t.tima, before + 1);
}

TEST(edge, select01_taps_div_bit3) {
    gb::TimerDevice t;
    t.write_tac(0x05);  // enabled, select 01 -> bit 3
    EXPECT_EQ(gb::TimerDevice::select_shift(t.tac), 3);
    const uint8_t before = t.tima;
    t.step(kPeriod[1]);
    EXPECT_EQ(t.tima, before + 1);
}

TEST(edge, select10_taps_div_bit5) {
    gb::TimerDevice t;
    t.write_tac(0x06);  // enabled, select 10 -> bit 5
    EXPECT_EQ(gb::TimerDevice::select_shift(t.tac), 5);
    const uint8_t before = t.tima;
    t.step(kPeriod[2]);
    EXPECT_EQ(t.tima, before + 1);
}

TEST(edge, select11_taps_div_bit7) {
    gb::TimerDevice t;
    t.write_tac(0x07);  // enabled, select 11 -> bit 7
    EXPECT_EQ(gb::TimerDevice::select_shift(t.tac), 7);
    const uint8_t before = t.tima;
    t.step(kPeriod[3]);
    EXPECT_EQ(t.tima, before + 1);
}

TEST(edge, disabled_timer_stops_incrementing) {
    gb::TimerDevice t;
    t.write_tac(0x05);
    t.step(160);
    EXPECT_EQ(t.tima, 10);
    t.write_tac(0x01);  // same select, gate OFF; disable edge handled below
    const uint8_t frozen = t.tima;
    t.step(400);
    EXPECT_EQ(t.tima, frozen);
}

TEST(edge, enable_mid_stream_counts_next_natural_fall) {
    // Start the divider so the tapped bit (bit 3 of select 01) already
    // reads 1, THEN enable. The next natural fall must still count --
    // sampling never stopped.
    gb::TimerDevice t;
    t.div.counter = 0x0008;   // bit 3 set
    t.prev_sample = true;
    t.write_tac(0x05);
    t.step(16);               // one full period: 1 -> 0 fall inside
    EXPECT_EQ(t.tima, 1);
}

TEST(edge, wrap_sets_overflow_pulse_and_wraps_to_zero) {
    // TIMA at $FF + one fall -> pulse; reload policy is exercise 03.
    gb::TimerDevice t;
    t.write_tac(0x05);
    t.tima = 0xFF;
    // Park the counter just below a bit-3 fall: bit 3 set, about to clear.
    t.div.counter = 0x0018;   // $18 + 4 == $1C: bit 3 still set...
    t.step(8);                // ...$20: fell once
    EXPECT_TRUE(t.overflow_pulse);
    EXPECT_EQ(t.tima, 0x00);
}
