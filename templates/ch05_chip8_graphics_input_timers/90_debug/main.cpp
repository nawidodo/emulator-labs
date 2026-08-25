#define LABSTEST_MAIN
#include "labstest.hpp"
#include "machine.hpp"

#include <array>

// Regression tests for the three seeded DXYN/timer defects. All three must
// pass once the bugs are found and fixed (and in the reference solution).

TEST(bug1_collision, vf_set_only_when_lit_pixel_erased) {
    chip8::Machine m;
    const std::array<uint8_t, 2> rows{0xF0, 0x00};
    // Clean draw onto empty screen: VF must stay 0.
    EXPECT_FALSE(chip8::draw_sprite(m.display, rows.data(), 1, 10, 10,
                                    m.quirks));
    // Erasing draw over the same spot: VF must be 1.
    EXPECT_TRUE(chip8::draw_sprite(m.display, rows.data(), 1, 10, 10,
                                   m.quirks));
    // Re-lighting draw: VF back to 0.
    EXPECT_FALSE(chip8::draw_sprite(m.display, rows.data(), 1, 10, 10,
                                    m.quirks));
}

TEST(bug1_collision, mixed_draw_reports_overlap_only) {
    chip8::Display d;
    chip8::Chip8Quirks q;
    // Sprite row 0x0F at origin x=4 lights screen columns 8-11.
    d.set(9, 4, true);   // pre-lit inside that span -> must erase + collide
    const std::array<uint8_t, 1> row{0x0F};
    EXPECT_TRUE(chip8::draw_sprite(d, row.data(), 1, 4, 4, q));
    EXPECT_FALSE(d.get(9, 4));   // erased
    EXPECT_TRUE(d.get(8, 4));    // freshly lit, no collision from it
    EXPECT_TRUE(d.get(10, 4));
}

TEST(bug2_clip, partial_row_still_draws_onscreen_part) {
    chip8::Machine m;
    const std::array<uint8_t, 1> row{0xF0};  // sprite columns 0-3 lit
    // Origin x=62: only screen columns 62-63 exist; both must render.
    m.display.clear();
    EXPECT_FALSE(chip8::draw_sprite(m.display, row.data(), 1, 62, 5,
                                    m.quirks));
    EXPECT_TRUE(m.display.get(62, 5));
    EXPECT_TRUE(m.display.get(63, 5));

    // Bottom edge: origin y=30 with a 4-row sprite keeps rows 30-31.
    const std::array<uint8_t, 4> rows{0xFF, 0xFF, 0xFF, 0xFF};
    m.display.clear();
    EXPECT_FALSE(chip8::draw_sprite(m.display, rows.data(), 4, 0, 30,
                                    m.quirks));
    EXPECT_TRUE(m.display.get(3, 30));
    EXPECT_TRUE(m.display.get(3, 31));
}

TEST(bug2_clip, fully_offscreen_row_draws_nothing) {
    chip8::Machine m;
    const std::array<uint8_t, 1> row{0xFF};
    EXPECT_FALSE(chip8::draw_sprite(m.display, row.data(), 1, 70, 5,
                                    m.quirks));
    EXPECT_FALSE(chip8::draw_sprite(m.display, row.data(), 1, 0, 40,
                                    m.quirks));
}

TEST(bug3_timers, one_tick_per_ten_cycles) {
    chip8::Timers t;
    t.set_delay(9);
    t.tick_cycles(10);            // exactly one 60 Hz tick
    EXPECT_EQ(t.delay, 8);
}

TEST(bug3_timers, sound_beeps_for_exactly_its_value_in_ticks) {
    chip8::Timers t;
    int end_ticks = -1;
    t.on_beep = [&](bool started) { if (!started) end_ticks = 0; };
    t.set_sound(3);
    uint64_t elapsed_ticks = 0;
    while (t.beeping() && elapsed_ticks < 100) {
        t.tick_cycles(10);
        ++elapsed_ticks;
        if (!t.beeping()) break;
    }
    // Sound=3 must beep for exactly 3 timer ticks = 30 cycles.
    EXPECT_EQ(elapsed_ticks * chip8::kCyclesPerTimerTick, 30u);
}

TEST(bug3_timers, delay_window_is_exact_across_seconds) {
    chip8::Timers t;
    t.set_delay(60);              // exactly one second at 60 Hz
    t.tick_cycles(chip8::kCyclesPerSecond);
    EXPECT_EQ(t.delay, 0);
}
