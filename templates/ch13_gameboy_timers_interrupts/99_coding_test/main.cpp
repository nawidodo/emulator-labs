// Coding-test regression pins for ch13: the three 90_debug edge cases,
// restated as direct behavioral assertions against the REAL chapter
// implementations (01-03). If a student "fixes" the debug excerpts but the
// main timer still has one of these defects, this suite catches it.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "../02_tima_edge/timer_dev.hpp"
#include "../03_overflow_reload/timer_irq.hpp"

namespace {

// Park a device with select 01 (DIV bit 3) enabled and the tapped bit high.
gb::TimerDevice sel01(uint8_t tima, uint8_t tma) {
    gb::TimerDevice t;
    t.write_tac(0x05);
    t.tima = tima;
    t.tma = tma;
    t.div.counter = 0x0018;
    return t;
}

}  // namespace

TEST(coding, gate_bit_is_tac_bit2_only) {
    // Enabled, select 00 ($04): must tick on DIV bit 9 falls.
    gb::TimerDevice on;
    on.write_tac(0x04);
    on.step(1024 * 4);
    EXPECT_EQ(on.tima, 4);
    // Select 01 with the gate CLEAR ($01): must not tick at all.
    gb::TimerDevice off;
    off.write_tac(0x01);
    off.step(16 * 100);
    EXPECT_EQ(off.tima, 0);
    EXPECT_FALSE(off.enabled());
}

TEST(coding, disable_with_tapped_bit_low_produces_one_tick) {
    gb::IntCtl ctl;
    gb::TimerDevice t = sel01(0x40, 0x00);  // tapped bit high, falls in 2 blocks
    timer_tick(t, ctl, 8);                  // consume the fall deterministically
    EXPECT_EQ(t.tima, 0x41);
    // Counter now sits where the tapped bit READS 0; disabling must add
    // exactly one increment (the disable edge), not zero, not two.
    t.write_tac(0x01);
    EXPECT_EQ(t.tima, 0x42);
    EXPECT_FALSE(t.enabled());
    // And it stays put afterwards.
    t.step(64);
    EXPECT_EQ(t.tima, 0x42);
}

TEST(coding, overflow_reloads_once_and_raises_if_once) {
    gb::IntCtl ctl;
    gb::TimerDevice t = sel01(0xFF, 0x37);
    EXPECT_TRUE(timer_tick(t, ctl, 16));   // exactly one overflow inside
    EXPECT_EQ(t.tima, 0x37);               // TMA value, not $00
    EXPECT_NE(ctl.flags & gb::kTimerIf, 0);
    // Settling twice must not double-raise: pulse already consumed.
    EXPECT_FALSE(t.overflow_pulse);
    gb::IntCtl ctl2;
    gb::settle_overflow(t, ctl2);
    EXPECT_EQ(ctl2.flags & gb::kTimerIf, 0);
}
