// Exercise 13.03 tests -- overflow reload policy and IF wiring.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "../02_tima_edge/timer_dev.hpp"
#include "timer_irq.hpp"

namespace {

// Enable select 01 (tapped DIV bit 3, one fall per 16 T-cycles) and park
// the counter so the tapped bit falls on the second 4-cycle block.
gb::TimerDevice armed(uint8_t tima, uint8_t tma) {
    gb::TimerDevice t;
    t.write_tac(0x05);
    t.tima = tima;
    t.tma = tma;
    t.div.counter = 0x0018;
    return t;
}

}  // namespace

TEST(reload, wraps_reload_immediately_from_tma) {
    gb::TimerDevice t = armed(0xFF, 0x37);
    gb::IntCtl ctl;
    EXPECT_FALSE(timer_tick(t, ctl, 4));  // tapped bit still high
    EXPECT_EQ(t.tima, 0xFF);
    EXPECT_TRUE(timer_tick(t, ctl, 4));   // the fall: 0xFF -> 0x00 -> TMA
    EXPECT_EQ(t.tima, 0x37);              // reloaded, NOT $00 -- immediate
}

TEST(reload, raises_if_bit2_on_overflow) {
    gb::TimerDevice t = armed(0xFF, 0x99);
    gb::IntCtl ctl;
    timer_tick(t, ctl, 16);               // one overflow inside
    EXPECT_NE(ctl.flags & gb::kTimerIf, 0);
    // Pulse was consumed by the settle step; counting resumes from TMA.
    timer_tick(t, ctl, 16);
    EXPECT_EQ(t.tima, 0x9A);
    EXPECT_FALSE(t.overflow_pulse);
}

TEST(reload, tma_write_affects_next_reload_only) {
    gb::IntCtl ctl;
    gb::TimerDevice t = armed(0xFF, 0x11);
    timer_tick(t, ctl, 16);
    EXPECT_EQ(t.tima, 0x11);   // loaded while TMA was $11
    t.tma = 0x42;              // does NOT retro-change the value loaded...
    t.tima = 0xFF;             // ...re-arm for the next overflow only
    t.div.counter = 0x0018;
    t.prev_sample = true;
    timer_tick(t, ctl, 16);
    EXPECT_EQ(t.tima, 0x42);   // next reload picks up the new TMA
}

TEST(reload, no_overflow_no_if_change) {
    gb::TimerDevice t = armed(0x10, 0x55);
    gb::IntCtl ctl;
    timer_tick(t, ctl, 32);    // two plain falls, nowhere near $FF
    EXPECT_EQ(ctl.flags & gb::kTimerIf, 0);
    EXPECT_EQ(t.tima, 0x12);
}
