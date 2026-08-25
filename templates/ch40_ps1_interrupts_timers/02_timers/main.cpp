#define LABSTEST_MAIN
#include "labstest.hpp"
#include "timers.hpp"

using ps1::sysdev::TimerBank;
using ps1::sysdev::TimerSignals;

namespace {

struct SinkLog {
    int count = 0;
    int last_timer = -1;
    // Only rising edges count as interrupt deliveries.
    static void fire(void* self, int timer, bool asserted) {
        if (!asserted) return;
        auto* log = static_cast<SinkLog*>(self);
        ++log->count;
        log->last_timer = timer;
    }
};

constexpr uint16_t kResetOnTarget = 1u << 3;
constexpr uint16_t kIrqTarget = 1u << 4;
constexpr uint16_t kIrqWrap = 1u << 5;
constexpr uint16_t kRepeat = 1u << 6;
constexpr uint16_t kToggle = 1u << 7;

void ticks(TimerBank& t, int n, const TimerSignals& s = {}, SinkLog* log = nullptr) {
    for (int i = 0; i < n; ++i)
        t.tick(s, log ? &SinkLog::fire : nullptr, log);
}

}  // namespace

TEST(timer_mode_write, forces_counter_to_zero) {
    TimerBank t;
    t.write_counter(0, 1234);
    t.write_mode(0, 0x0008u);                       // reset@target
    EXPECT_EQ(t.regs[0].counter, 0u);
    // Bit 10 reads "no request" right after a MODE write.
    EXPECT_TRUE(t.read_mode(0) & ps1::sysdev::kModeFlagIrqRequest);
}

TEST(timer_clocks, sysclk_increments_every_cycle) {
    TimerBank t;
    t.write_mode(1, 0x0000u);                       // timer1, free-run sysclk
    ticks(t, 7);
    EXPECT_EQ(t.regs[1].counter, 7u);
}

TEST(timer_clocks, sysclk_div8_increments_every_eighth) {
    TimerBank t;
    t.write_mode(2, 0x0200u);                       // timer2 source=2 -> /8
    ticks(t, 16);
    EXPECT_EQ(t.regs[2].counter, 2u);
}

TEST(timer_clocks, dotclock_counts_pulses_only) {
    TimerBank t;
    t.write_mode(0, 0x0100u);                       // timer0 source=1 -> dot
    TimerSignals s;
    for (int i = 0; i < 12; ++i) {
        s.dot_pulse = (i % 3 == 0);                 // pulse every third cycle
        t.tick(s, nullptr, nullptr);
    }
    EXPECT_EQ(t.regs[0].counter, 4u);
}

TEST(timer_clocks, hblank_source_timer0_sel3) {
    TimerBank t;
    t.write_mode(0, 0x0300u);                       // timer0 source=3 -> hblank
    TimerSignals s;
    s.hblank_pulse = true;
    ticks(t, 5, s);
    EXPECT_EQ(t.regs[0].counter, 5u);
    s.hblank_pulse = false;
    ticks(t, 50, s);
    EXPECT_EQ(t.regs[0].counter, 5u);               // only hblanks advance it
}

TEST(timer_target, resets_at_target_when_bit3_set) {
    TimerBank t;
    t.write_target(0, 10);
    t.write_mode(0, kResetOnTarget);                // sysclk, reset@target
    ticks(t, 25);
    EXPECT_EQ(t.regs[0].counter, 5u);               // periods of exactly 10
}

TEST(timer_target, wraps_at_ffff_without_bit3) {
    TimerBank t;
    t.write_mode(0, 0x0000u);                       // no reset bit
    t.write_counter(0, 0xFFFEu);                    // counter write after MODE
    TimerSignals s;
    t.tick(s, nullptr, nullptr);                    // FFFE -> FFFF
    EXPECT_EQ(t.regs[0].counter, 0xFFFFu);
    t.tick(s, nullptr, nullptr);                    // FFFF -> wrap to 0
    EXPECT_EQ(t.regs[0].counter, 0u);
    EXPECT_TRUE(t.read_mode(0) & ps1::sysdev::kModeFlagReachedWrap);
    // Sticky flags clear on read.
    EXPECT_FALSE(t.read_mode(0) &
                 (ps1::sysdev::kModeFlagReachedWrap |
                  ps1::sysdev::kModeFlagReachedTarget));
}

TEST(timer_irq, fires_on_target_only_when_enabled) {
    TimerBank t;
    t.write_target(0, 4);
    t.write_mode(0, kResetOnTarget | kIrqTarget | kRepeat);
    SinkLog log;
    ticks(t, 10, {}, &log);
    EXPECT_EQ(log.count, 2);                        // one per 4-cycle period
    EXPECT_EQ(log.last_timer, 0);

    TimerBank quiet;
    quiet.write_target(0, 4);
    quiet.write_mode(0, kResetOnTarget | kRepeat);  // IRQ on target DISABLED
    SinkLog silent;
    ticks(quiet, 12, {}, &silent);
    EXPECT_EQ(silent.count, 0);
}

TEST(timer_irq, one_shot_suppresses_until_mode_rewrite) {
    TimerBank t;
    t.write_target(0, 4);
    t.write_mode(0, kResetOnTarget | kIrqTarget);   // repeat bit CLEAR
    SinkLog log;
    ticks(t, 16, {}, &log);
    EXPECT_EQ(log.count, 1);
    t.write_mode(0, kResetOnTarget | kIrqTarget);   // rearm via MODE write
    ticks(t, 8, {}, &log);
    EXPECT_EQ(log.count, 2);
}

TEST(timer_irq, repeat_fires_every_period) {
    TimerBank t;
    t.write_target(0, 4);
    t.write_mode(0, kResetOnTarget | kIrqTarget | kRepeat);
    SinkLog log;
    ticks(t, 20, {}, &log);
    EXPECT_EQ(log.count, 5);
}

TEST(timer_irq, wrap_irq_uses_ffff_event) {
    TimerBank t;
    t.write_mode(1, kIrqWrap | kRepeat);            // timer1, no reset bit
    t.write_counter(1, 0xFFFDu);
    SinkLog log;
    ticks(t, 5, {}, &log);
    EXPECT_EQ(log.count, 1);
    EXPECT_EQ(log.last_timer, 1);
    EXPECT_EQ(t.regs[1].counter, 2u);               // FFFE, FFFF, 0, 1, 2
}

TEST(timer_irq, toggle_mode_delivers_every_second_event) {
    TimerBank t;
    t.write_target(0, 4);
    t.write_mode(0, kResetOnTarget | kIrqTarget | kRepeat | kToggle);
    SinkLog log;
    ticks(t, 32, {}, &log);                         // 8 target events
    EXPECT_EQ(log.count, 4);                        // every second one
    // Bit 10 reads 0 ("requesting") exactly while the line is asserted.
    uint16_t m = t.read_mode(0);
    EXPECT_EQ((m & ps1::sysdev::kModeFlagIrqRequest) != 0u, !t.irq_line(0));
}

TEST(timer_flags, reached_target_sticky_until_read) {
    TimerBank t;
    t.write_target(0, 4);
    t.write_mode(0, kResetOnTarget);                // no IRQ at all
    ticks(t, 9);
    EXPECT_TRUE(t.read_mode(0) & ps1::sysdev::kModeFlagReachedTarget);
    EXPECT_FALSE(t.read_mode(0) & ps1::sysdev::kModeFlagReachedTarget);
}

TEST(timer_sync, timer0_pause_during_hblank) {
    TimerBank t;
    t.write_mode(0, 0x0001u);                       // sync enable, mode 0
    TimerSignals s;
    s.hblank_level = true;
    ticks(t, 10, s);
    EXPECT_EQ(t.regs[0].counter, 0u);               // frozen inside hblank
    s.hblank_level = false;
    ticks(t, 6, s);
    EXPECT_EQ(t.regs[0].counter, 6u);
}

TEST(timer_sync, timer0_reset_and_count_inside_hblank) {
    TimerBank t;
    t.write_mode(0, 0x0005u);                       // sync enable, mode 2
    TimerSignals s;
    s.hblank_level = false;
    ticks(t, 30, s);                                // outside: paused
    EXPECT_EQ(t.regs[0].counter, 0u);
    s.hblank_level = true;
    ticks(t, 7, s);                                 // inside: counts every tick
    // The rising edge resets first, then all seven ticks count.
    EXPECT_EQ(t.regs[0].counter, 7u);
}

TEST(timer_sync, timer0_wait_for_first_hblank_then_free_run) {
    TimerBank t;
    t.write_mode(0, 0x0007u);                       // sync enable, mode 3
    TimerSignals s;
    ticks(t, 20, s);
    EXPECT_EQ(t.regs[0].counter, 0u);               // still waiting
    s.hblank_level = true;
    ticks(t, 4, s);                                 // hblank begins...
    s.hblank_level = false;
    ticks(t, 5, s);                                 // ...ends: free-running
    EXPECT_EQ(t.regs[0].counter, 9u);               // 4 inside + 5 outside
}

TEST(timer_sync, timer1_resets_on_vblank_start) {
    TimerBank t;
    t.write_mode(1, 0x0003u);                       // sync enable, mode 1
    TimerSignals s;
    ticks(t, 9, s);
    s.vblank_level = true;                          // rising edge: reset + count
    ticks(t, 3, s);
    s.vblank_level = false;
    // Edge resets the 9, then all three vblank cycles count (mode 1 never
    // pauses), then vblank end is just a level change.
    EXPECT_EQ(t.regs[1].counter, 3u);
}

TEST(timer_sync, timer2_stops_forever_in_modes_0_and_3) {
    TimerBank t;
    t.write_mode(2, 0x0001u);                       // sync enabled, mode 0
    TimerSignals s;
    ticks(t, 10, s);
    EXPECT_EQ(t.regs[2].counter, 0u);               // stopped forever
    t.write_mode(2, 0x0003u);                       // mode 1 -> free run
    ticks(t, 4, s);
    EXPECT_EQ(t.regs[2].counter, 4u);
}
