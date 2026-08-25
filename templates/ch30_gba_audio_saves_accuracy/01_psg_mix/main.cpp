#define LABSTEST_MAIN
#include "labstest.hpp"
#include "psg.hpp"

using namespace gba;

TEST(duty, wave_shapes) {
    // 50% duty: four high then four low.
    EXPECT_TRUE(psg_duty_high(2, 0));
    EXPECT_TRUE(psg_duty_high(2, 3));
    EXPECT_FALSE(psg_duty_high(2, 4));
    // 12.5%: only step 7 high with our mask orientation (LSB-first phase).
    int high = 0;
    for (int s = 0; s < 8; ++s) high += psg_duty_high(0, s);
    EXPECT_EQ(high, 1);
    int h75 = 0;
    for (int s = 0; s < 8; ++s) h75 += psg_duty_high(3, s);
    EXPECT_EQ(h75, 6);  // mask 0xFC has six set bits above bit1
    // Phase wraps.
    EXPECT_EQ(psg_duty_high(2, 8), psg_duty_high(2, 0));
}

TEST(lfsr, known_sequence_and_short_mode) {
    u16 st = 0x7FFF;
    int out = lfsr_step(st, false);
    // Deterministic: first output is 0 (low bit becomes 1), state changes.
    EXPECT_EQ(out, 0);
    EXPECT_NE(st, 0x7FFF);
    // Short mode keeps the fold at bit 6: after one step bit pattern differs
    // from long mode given the same start.
    u16 a = 0x7FFF, b = 0x7FFF;
    lfsr_step(a, false);
    lfsr_step(b, true);
    EXPECT_NE(a, b);
}

TEST(envelope, decay_increase_and_disable) {
    // Decay from 15, period 4: after 8 clocks -> 13.
    EXPECT_EQ(envelope_volume(15, 4, 0, 8), 13);
    // Increase from 5, period 2: after 6 clocks -> 8.
    EXPECT_EQ(envelope_volume(5, 2, 1, 6), 8);
    // Clamps at bounds.
    EXPECT_EQ(envelope_volume(3, 1, 0, 100), 0);
    EXPECT_EQ(envelope_volume(14, 1, 1, 100), 15);
    // Period 0 disables.
    EXPECT_EQ(envelope_volume(9, 0, 0, 50), 9);
}

TEST(mixer, routing_volumes_clamp) {
    int ch[4] = {10, -4, 6, 0};
    int l, r;
    // All channels right only, volume 0 (scale 1).
    psg_mix(ch, u16(0 | 0 << 4), u16(0x0F), &l, &r);
    EXPECT_EQ(r, 12);
    EXPECT_EQ(l, 0);
    // Left gets all too, volume 7 -> scale 8: (10-4+6)*8 = 96.
    psg_mix(ch, u16((7) << 4 | 7), u16(0xFF), &l, &r);
    EXPECT_EQ(l, 96);
    // Clamp: huge positives cap at 511.
    int big[4] = {15, 15, 15, 15};
    psg_mix(big, u16(7 | 7 << 4), u16(0x000F), &l, &r);
    EXPECT_EQ(r, 480);  // 60 * 8
}
