#define LABSTEST_MAIN
#include "labstest.hpp"
#include "timers.hpp"

using namespace gba;

TEST(prescale, field_to_cycles) {
    EXPECT_EQ(prescale_cycles(0), 1u);
    EXPECT_EQ(prescale_cycles(1), 64u);
    EXPECT_EQ(prescale_cycles(2), 256u);
    EXPECT_EQ(prescale_cycles(3), 1024u);
}

TEST(timer, exact_reload_period) {
    Timer t;
    t.reload = 0xFFF0;               // period = 0x10 ticks
    t.counter = 0x0000;              // fresh start counts up from zero
    t.control = 0x80;                // enable, prescaler 1
    u64 rem = 0;
    int ovf = timer_tick(t, 0xF, &rem);
    EXPECT_EQ(ovf, 0);
    EXPECT_EQ(t.counter, 0x000F);
    // Jump next to the top, then cross it: wrap lands on the RELOAD value.
    t.counter = 0xFFFF;
    ovf = timer_tick(t, 1, &rem);
    EXPECT_EQ(ovf, 1);
    EXPECT_EQ(t.counter, 0xFFF0);
}

TEST(timer, phase_keeps_across_partial_calls) {
    Timer t;
    t.reload = 5;                    // period 0xFFFB... use small reload:
    t.counter = 0xFFFE;              // two ticks to overflow
    t.control = 0x81;                // prescaler 64
    u64 rem = 0;
    int ovf = timer_tick(t, 64, &rem);   // one tick
    EXPECT_EQ(ovf, 0);
    EXPECT_EQ(t.counter, 0xFFFF);
    ovf = timer_tick(t, 63, &rem);       // sub-tick remainder only
    EXPECT_EQ(ovf, 0);
    EXPECT_EQ(rem, 63u);
    ovf = timer_tick(t, 65, &rem);       // 1 tick + 1 leftover
    EXPECT_EQ(ovf, 1);
    EXPECT_EQ(t.counter, 5);
    EXPECT_EQ(rem, 1u);
}

TEST(cascade, divides_by_chained_overflow) {

    Timer t[4];
    // Timer 0: reload 0xFFFF (period 1 tick), parked one below the top.
    t[0].reload = 0xFFFF;
    t[0].counter = 0xFFFF;
    t[0].control = 0x80;
    // Timer 1: cascade on timer 0, reload 0xFFFE (period 2 ticks).
    t[1].counter = 0xFFFE;
    t[1].reload = 0xFFFE;
    t[1].control = (1 << 2) | 0x80;  // cascade + enable

    u16 irq = 0;
    int last = chain_tick(t, 4, irq);  // 4 guest cycles -> 4 t0 ticks
    // t0 overflowed 4 times; t1 ticked 4 times -> 2 overflows.
    EXPECT_EQ(last, 2);
    EXPECT_EQ(irq & 1, 0);   // t0 IRQ disabled
    // Enable t1 IRQ and confirm the flag.
    t[1].control |= 1 << 6;
    t[1].counter = 0xFFFE;
    t[0].counter = 0xFFFF;
    irq = 0;
    chain_tick(t, 6, irq);
    EXPECT_NE(irq & 2, 0);

    // Prescaled root: timer0 at 1024 cycles/tick sees nothing in 100 cycles.
    Timer s[4];
    s[0].reload = 0xFFFF;
    s[0].control = u16(0x80 | 3);
    u16 f2 = 0;
    EXPECT_EQ(chain_tick(s, 100, f2), 0);
}
