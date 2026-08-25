#define LABSTEST_MAIN
#include "labstest.hpp"
#include "timers.hpp"
#include <cstddef>

TEST(timers, start_at_zero_and_stay_there) {
    chip8::Timers t;
    EXPECT_EQ(t.delay, 0);
    EXPECT_EQ(t.sound, 0);
    t.tick_cycles(6000);  // 100 ticks with nothing set: still zero
    EXPECT_EQ(t.delay, 0);
    EXPECT_EQ(t.sound, 0);
    EXPECT_FALSE(t.beeping());
}

TEST(timers, one_tick_per_ten_cycles) {
    chip8::Timers t;
    t.set_delay(5);
    t.tick_cycles(9);
    EXPECT_EQ(t.delay, 5);   // not yet a full tick window
    t.tick_cycles(1);
    EXPECT_EQ(t.delay, 4);   // exactly 10 cycles -> one tick
}

TEST(timers, sixty_hz_means_600_cycles_is_60_ticks_per_second) {
    // One second of CPU time = 600 cycles = 60 timer ticks.
    chip8::Timers t;
    t.set_delay(200);
    t.set_sound(100);
    t.tick_cycles(chip8::kCyclesPerSecond);
    EXPECT_EQ(t.delay, 140);
    EXPECT_EQ(t.sound, 40);
}

TEST(timers, arbitrary_cycle_counts_accumulate) {
    chip8::Timers t;
    t.set_delay(10);
    for (int i = 0; i < 7; ++i) t.tick_cycles(1);   // 7 cycles banked
    EXPECT_EQ(t.delay, 10);
    t.tick_cycles(3);                               // now 10 -> one tick
    EXPECT_EQ(t.delay, 9);
    t.tick_cycles(20);                              // 20 -> two more ticks
    EXPECT_EQ(t.delay, 7);
}

TEST(timers, clamps_at_zero) {
    chip8::Timers t;
    t.set_delay(3);
    t.set_sound(2);
    t.tick_cycles(6000);  // far more than enough
    EXPECT_EQ(t.delay, 0);
    EXPECT_EQ(t.sound, 0);
    EXPECT_FALSE(t.beeping());
}

TEST(timers, beep_start_fires_when_sound_set_nonzero) {
    chip8::Timers t;
    chip8::BeepRecorder rec;
    rec.attach(t);
    t.set_sound(5);
    EXPECT_TRUE(rec.started_count_equals(1));
    EXPECT_EQ(rec.events.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(rec.events[0]);
}

TEST(timers, beep_end_fires_exactly_when_sound_hits_zero) {
    chip8::Timers t;
    chip8::BeepRecorder rec;
    rec.attach(t);
    t.set_sound(2);
    t.tick_cycles(19);          // one tick: sound 2 -> 1, still beeping
    EXPECT_TRUE(t.beeping());
    EXPECT_EQ(rec.events.size(), static_cast<std::size_t>(1));
    t.tick_cycles(1);           // second tick: sound reaches 0
    EXPECT_FALSE(t.beeping());
    EXPECT_EQ(rec.events.size(), static_cast<std::size_t>(2));
    EXPECT_FALSE(rec.events[1]);
}

TEST(timers, setting_sound_while_beeping_fires_nothing_new) {
    chip8::Timers t;
    chip8::BeepRecorder rec;
    rec.attach(t);
    t.set_sound(4);
    t.set_sound(6);             // top-up: no new "start"
    t.tick_cycles(30);          // three ticks: 6 -> 3, never hit zero
    EXPECT_EQ(rec.events.size(), static_cast<std::size_t>(1));
}

TEST(timers, delay_tick_does_not_fire_beep_hook) {
    chip8::Timers t;
    chip8::BeepRecorder rec;
    rec.attach(t);
    t.set_delay(5);
    t.tick_cycles(50);          // five ticks, delay drains to 0
    EXPECT_EQ(t.delay, 0);
    EXPECT_TRUE(rec.events.empty());
}

TEST(timers, separate_rates_decoupled_by_accumulator) {
    // The point of TODO5 in the curriculum: CPU rate and timer rate are
    // independent. Half a second of CPU cycles must produce exactly half a
    // second worth of timer ticks regardless of how the cycles are fed in.
    chip8::Timers a;
    a.set_delay(100);
    a.tick_cycles(chip8::kCyclesPerSecond / 2);   // one call, 300 cycles
    EXPECT_EQ(a.delay, 70);

    chip8::Timers b;
    b.set_delay(100);
    for (uint32_t i = 0; i < chip8::kCyclesPerSecond / 2; ++i)
        b.tick_cycles(1);                          // 300 single-cycle calls
    EXPECT_EQ(b.delay, 70);
}
