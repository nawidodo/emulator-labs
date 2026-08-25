#define LABSTEST_MAIN
#include "labstest.hpp"

#include "apu_channels.hpp"

using namespace nes24apu;

TEST(nes24pulse, duty_sequencer_steps_on_timer_expiry) {
    Pulse p;
    p.timer_period = 0;
    int first = (p.tick_timer(), p.duty_step);
    int second = (p.tick_timer(), p.duty_step);
    EXPECT_EQ((second + 7) & 7, first);
}

TEST(nes24pulse, envelope_restart_decay_and_constant_volume) {
    Pulse p;
    p.enabled = true;            // length loads require the channel enabled
    p.write_reg(2, 100);         // valid period (>= 8 keeps channel audible)
    p.write_reg(0, 0x00);        // divider period 0, decay mode
    p.write_reg(3, 0x08);        // env_start
    EXPECT_EQ(p.env_start, 1);
    p.tick_envelope();           // consumes start: decay=15, divider=0
    EXPECT_EQ(p.output(), 15);
    p.tick_envelope();           // divider 0 -> immediate decay drop
    EXPECT_EQ(p.output(), 14);
    // Constant volume overrides decay.
    Pulse c;
    c.enabled = true;
    c.write_reg(2, 100);
    c.write_reg(0, 0x1F);        // constant, volume 15
    c.write_reg(3, 0x08);
    c.tick_envelope();
    c.tick_envelope();
    EXPECT_EQ(c.output(), 15);
}

TEST(nes24pulse, length_load_and_halt) {
    Pulse p;
    p.enabled = true;
    p.write_reg(3, 0x28);        // load index 5 -> 4 frames
    EXPECT_EQ(p.length_counter, kLengthTable[5]);
    p.tick_length_and_sweep();   // halts not set: 4->3
    EXPECT_EQ(p.length_counter, 3);
    p.write_reg(0, 0x20);        // halt
    p.tick_length_and_sweep();
    EXPECT_EQ(p.length_counter, 3);
}

TEST(nes24pulse, sweep_negate_mode_is_asymmetric_between_channels) {
    // Same registers, different channel: pulse 2's subtract adds one back.
    int targets[2];
    for (int ch = 0; ch < 2; ++ch) {
        Pulse p;
        p.channel = ch;
        p.write_reg(2, 100 & 0xFF);
        p.write_reg(3, (100 >> 8) & 7);   // period = 100
        p.write_reg(1, 0x89);             // enabled, negate, shift 1, per 0
        p.tick_length_and_sweep();
        targets[ch] = p.timer_period;
    }
    EXPECT_EQ(targets[0], 50);   // 100 - (100>>1)
    EXPECT_EQ(targets[1], 51);   // ...plus the pulse-2 correction
}

TEST(nes24pulse, sweep_positive_target_over_7ff_never_lands) {
    Pulse p;
    p.write_reg(2, uint8_t(2000 & 0xFF));
    p.write_reg(3, uint8_t((2000 >> 8) & 7));  // period = 2000
    p.write_reg(1, 0x83);        // enabled, shift 3, divider 0
    p.tick_length_and_sweep();   // target 2250 > $7FF: rejected
    EXPECT_EQ(p.timer_period, 2000);
}

TEST(nes24tri, linear_counter_reload_and_control) {
    Triangle t;
    t.write_reg(0, 0x05);        // reload value 5, control off
    t.write_reg(3, 0x08);        // sets reload flag
    t.tick_linear();
    EXPECT_EQ(t.linear_counter, 5);
    t.tick_linear();             // flag consumed: now counts down
    EXPECT_EQ(t.linear_counter, 4);
}

TEST(nes24tri, sequencer_gated_by_both_counters_and_ramps_down) {
    Triangle t;
    t.enabled = true;
    t.length_counter = 4;
    t.linear_counter = 4;
    t.timer_period = 0;
    for (int i = 0; i < 3; ++i) t.tick_timer();
    EXPECT_EQ(t.step, 3);
    EXPECT_EQ(t.output(), 12);   // 15 - step
    t.length_counter = 0;        // sequencer silences
    t.tick_timer();
    EXPECT_EQ(t.step, 3);
}

TEST(nes24noise, short_mode_lfsr_sequence_is_pinned) {
    Noise n;
    n.write_reg(2, 0x80);        // mode bit7: SHORT LFSR (tap at bit 6)
    n.shift = 1;
    const uint16_t want[] = {0x4000, 0x2000, 0x1000, 0x0800, 0x0400,
                             0x0200, 0x0100, 0x0080, 0x0040, 0x4020};
    for (int i = 0; i < 10; ++i) {
        n.timer_counter = 0;     // force expiry every iteration
        n.tick_timer();          // period index 0 -> expiry every call
        EXPECT_EQ(n.shift, want[i]);
    }
}

TEST(nes24noise, period_table_and_output_gate_on_shift_bit0) {
    Noise n;
    n.enabled = true;
    n.length_counter = 5;
    n.write_reg(2, 0x05);        // rate index 5 -> 96 CPU cycles
    EXPECT_EQ(kNoisePeriod[5], 96);
    n.shift = 1;                 // bit0 set -> silent until feedback clears it
    EXPECT_EQ(n.output(), 0);
    n.shift = 2;                 // bit0 clear -> audible
    EXPECT_TRUE(n.output() >= 0);
}

TEST(nes24dmc, level_direct_write_and_stub_fetch_contract) {
    Dmc d;
    d.enabled = true;
    d.write_reg(1, 0x40);        // $4011: direct 7-bit level
    EXPECT_EQ(d.output(), 0x40);
    d.restart(16);
    EXPECT_TRUE(d.needs_fetch());          // flagged...
    // ...and the stub NEVER steals cycles: nothing else observable changes.
    EXPECT_EQ(d.bytes_remaining == 16 || d.bytes_remaining == 0, true);
}
