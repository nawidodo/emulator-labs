// Tests for exercise 01: pulse channel sequencing.
//
// All expectations follow the conventions committed in square.hpp/SPEC.md;
// every sequence is walked by injecting cycles or frame-sequencer ticks
// directly, so the suite is fully deterministic.
#define LABSTEST_MAIN
#include <cstdio>

#include "labstest.hpp"
#include "square.hpp"

using gbaudio::SquareChannel;

namespace {

// Channel prepped for duty walking: 50% duty, volume 15, envelope frozen,
// freq 2047 -> shortest timer period (4 T-cycles per duty step).
SquareChannel makeWalker() {
    SquareChannel ch{false};
    ch.writeNR11(0x80);  // duty 2 (50%)
    ch.writeNR12(0xF0);  // initial volume 15, envelope period 0
    ch.writeNR13(0xFF);
    ch.writeNR14(0x87);  // freq hi bits + trigger
    return ch;
}
}  // namespace

TEST(square_duty, forms_are_msb_first_rows) {
    // Committed duty table: bit (7 - position) of the row byte.
    const uint8_t forms[4] = {0x01, 0x03, 0x0F, 0x3F};
    for (int d = 0; d < 4; ++d)
        for (int p = 0; p < 8; ++p) {
            const int want =
                static_cast<int>((forms[d] >> (7 - p)) & 1);
            EXPECT_EQ(
                static_cast<int>((gbaudio::kDutyForms[d] >> (7 - p)) & 1),
                want);
        }
}

TEST(square_duty, advance_walks_position_every_period) {
    SquareChannel ch = makeWalker();
    EXPECT_TRUE(ch.enabled);
    EXPECT_EQ(ch.dutyPos, 0);
    for (int step = 1; step <= 16; ++step) {
        ch.advance(4);  // exactly one timer period at freq 2047
        EXPECT_EQ(ch.dutyPos, step & 7);
    }
}

TEST(square_duty, sample_selects_volume_on_duty_bit) {
    SquareChannel ch = makeWalker();
    // Committed formula sample=(form>>(7-p))&1: the 50% row 0x0F reads
    // LOW on positions 0..3 and HIGH (volume 15) on positions 4..7.
    for (int p = 0; p < 8; ++p) {
        ch.dutyPos = p;
        EXPECT_EQ(ch.sample(), (p < 4 ? 0 : 15));
    }
}

TEST(square_length, counts_down_and_disables_at_zero) {
    SquareChannel ch{false};
    ch.lengthEnabled = true;
    ch.lengthCounter = 3;
    ch.enabled = true;
    ch.lengthTick();
    EXPECT_TRUE(ch.enabled);
    EXPECT_EQ(ch.lengthCounter, 2);
    ch.lengthTick();
    ch.lengthTick();
    EXPECT_FALSE(ch.enabled);
    EXPECT_EQ(ch.lengthCounter, 0);
    EXPECT_EQ(ch.sample(), 0);  // disabled channels are silent
}

TEST(square_length, trigger_reloads_expired_counter_to_64) {
    SquareChannel ch{false};
    ch.lengthEnabled = true;
    ch.lengthCounter = 0;
    ch.trigger();
    EXPECT_EQ(ch.lengthCounter, gbaudio::kSquareLengthMax);
    EXPECT_TRUE(ch.enabled);
    // 63 more length ticks keep it alive; the 64th expires exactly.
    for (int i = 0; i < 63; ++i) ch.lengthTick();
    EXPECT_TRUE(ch.enabled);
    ch.lengthTick();
    EXPECT_FALSE(ch.enabled);
}

TEST(square_envelope, decays_by_period_and_clamps) {
    SquareChannel ch{false};
    ch.writeNR12(0x72);  // initial volume 7, decrease, period 2
    ch.trigger();
    const int want[] = {7, 6, 6, 5, 5, 4};
    for (int t = 0; t < 6; ++t) {
        ch.envelopeTick();
        EXPECT_EQ(ch.envVolume, want[t]);
    }
    // Run far past zero: clamped, never negative.
    for (int t = 0; t < 200; ++t) ch.envelopeTick();
    EXPECT_EQ(ch.envVolume, 0);
}

TEST(square_envelope, increases_and_clamps_at_15) {
    SquareChannel ch{false};
    ch.writeNR12(0x99);  // initial volume 9, increase, period 1
    ch.trigger();
    for (int v = 10; v <= 15; ++v) {
        ch.envelopeTick();
        EXPECT_EQ(ch.envVolume, v);
    }
    ch.envelopeTick();
    EXPECT_EQ(ch.envVolume, 15);
}

TEST(square_envelope, period_zero_freezes_volume) {
    SquareChannel ch{false};
    ch.writeNR12(0xC0);  // initial volume 12, increase, period 0
    ch.trigger();
    for (int t = 0; t < 32; ++t) ch.envelopeTick();
    EXPECT_EQ(ch.envVolume, 12);
}

TEST(square_sweep, immediate_overflow_disables_at_trigger) {
    SquareChannel ch{true};
    ch.writeNR10(0x10);  // pace 1, positive, slope 0
    ch.freq = 2000;
    ch.trigger();  // first candidate 2000 + 2000 = 4000 > 2047
    EXPECT_FALSE(ch.enabled);
    EXPECT_EQ(ch.sample(), 0);
}

TEST(square_sweep, negative_mode_descends_via_shadow) {
    SquareChannel ch{true};
    ch.writeNR10(0x19);  // pace 1, negate, slope 1
    ch.freq = 100;
    ch.trigger();  // pending candidate 100 - 50 = 50
    EXPECT_TRUE(ch.hasPendingCandidate);
    ch.sweepTick();  // applies 50; next candidate 50 - 25 = 25
    EXPECT_EQ(ch.freq, 50);
    EXPECT_EQ(ch.shadowFreq, 50);
    EXPECT_EQ(ch.pendingCandidate, 25);
    ch.sweepTick();  // applies 25; next candidate 25 - 12 = 13
    EXPECT_EQ(ch.freq, 25);
    EXPECT_EQ(ch.pendingCandidate, 13);
    EXPECT_TRUE(ch.enabled);
}

TEST(square_sweep, negative_candidate_below_zero_is_discarded) {
    SquareChannel ch{true};
    ch.writeNR10(0x18);  // pace 1, negate, slope 0
    ch.freq = 1;
    ch.trigger();  // pending 1 - 1 = 0
    ch.sweepTick();  // applies 0; next candidate 0 - 1 = -1
    EXPECT_EQ(ch.freq, 0);
    EXPECT_TRUE(ch.enabled);
    ch.sweepTick();  // -1 discarded, frequency stays put
    EXPECT_EQ(ch.freq, 0);
    EXPECT_TRUE(ch.enabled);
}

TEST(square_sweep, second_update_overflow_disables) {
    SquareChannel ch{true};
    ch.writeNR10(0x12);  // pace 1, positive, slope 2
    ch.freq = 1500;
    ch.trigger();  // pending 1500 + 375 = 1875 (< 2047)
    EXPECT_TRUE(ch.enabled);
    ch.sweepTick();  // applies 1875; second candidate 2343 > 2047
    EXPECT_EQ(ch.freq, 1875);
    EXPECT_FALSE(ch.enabled);
}

TEST(square_sweep, pace_zero_is_inert) {
    SquareChannel ch{true};
    ch.writeNR10(0x02);  // pace 0 -> sweep disabled
    ch.freq = 1000;
    ch.trigger();
    EXPECT_FALSE(ch.hasPendingCandidate);
    for (int i = 0; i < 16; ++i) ch.sweepTick();
    EXPECT_EQ(ch.freq, 1000);
    EXPECT_TRUE(ch.enabled);
}

TEST(square_regs, dac_off_write_disables_channel) {
    SquareChannel ch = makeWalker();
    EXPECT_TRUE(ch.dacEnabled());
    ch.writeNR12(0x07);  // only envelope period bits set -> DAC off
    EXPECT_FALSE(ch.dacEnabled());
    EXPECT_FALSE(ch.enabled);
}
