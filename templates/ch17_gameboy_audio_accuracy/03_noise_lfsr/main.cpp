// Tests for exercise 03: LFSR noise channel.
//
// The "exact polynomial table" fixture — the first 64 raw output bits for
// divisor code 0 / s = 0 / width 15 from a fresh (0x7FFF) register — is
// committed as tests/public/ch17_gameboy_audio_accuracy/fixtures/
// noise_lfsr_div0_s0_w15.txt and duplicated inline here; provenance.md
// documents its generation. The width-7 row below comes from the same
// reference implementation and cross-checks both modes in one table.
#define LABSTEST_MAIN
#include <cstdio>
#include <cstring>

#include "labstest.hpp"
#include "noise.hpp"

using gbaudio::NoiseChannel;

namespace {

const char kPolyBitsWidth15[65] =
    "0000000000000011111111111111011111111111110011111111111101011111";
const char kPolyBitsWidth7[65] =
    "0000001111110111110011110101110000110111010011000101011000001011";

NoiseChannel makeChannel(uint8_t nr43) {
    NoiseChannel n;
    n.writeNR42(0xF0);   // volume 15, envelope frozen
    n.nr43 = nr43;
    n.writeNR44(0x80);   // trigger
    return n;
}

}  // namespace

TEST(noise_period, divisor_table_and_shifts) {
    const uint8_t codes[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint32_t want[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    for (int c = 0; c < 8; ++c) {
        NoiseChannel n;
        n.nr43 = codes[c];
        EXPECT_EQ(n.stepPeriod(), want[c]);
    }
    // s scales by powers of two: code 0 with s = 0..7.
    for (int s = 0; s <= 7; ++s) {
        NoiseChannel n;
        n.nr43 = static_cast<uint8_t>(s << 4);
        EXPECT_EQ(n.stepPeriod(),
                  static_cast<uint32_t>(8u << s));
    }
}

TEST(noise_lfsr, matches_exact_polynomial_table_width15) {
    // Raw output bits (~lfsr & 1 after each step), width-15 mode.
    NoiseChannel n = makeChannel(0x00);  // divisor 0, s 0, width 15
    for (int i = 0; i < 64; ++i) {
        n.step();
        const char bit = (~n.lfsr & 1) ? '1' : '0';
        EXPECT_EQ(bit, kPolyBitsWidth15[i]);
    }
}

TEST(noise_lfsr, matches_exact_polynomial_table_width7) {
    NoiseChannel n = makeChannel(0x08);  // divisor 0, s 0, width 7
    for (int i = 0; i < 64; ++i) {
        n.step();
        const char bit = (~n.lfsr & 1) ? '1' : '0';
        EXPECT_EQ(bit, kPolyBitsWidth7[i]);
    }
}

TEST(noise_lfsr, width7_taps_bit14_and_bit6_alike) {
    NoiseChannel n = makeChannel(0x08);
    n.lfsr = 0x0001;
    n.step();  // x = (1 ^ 0) & 1 = 1 -> bits 14 AND 6 become 1
    EXPECT_EQ(n.lfsr & 0x4000, 0x4000);
    EXPECT_EQ(n.lfsr & 0x40, 0x40);
}

TEST(noise_sample, inverted_bit0_times_envelope_volume) {
    NoiseChannel n = makeChannel(0x00);  // volume 15
    const int want[16] = {0,  0,  0,  0,  0,  0,  0,  0,
                          0,  0,  0,  0,  0,  0,  15, 15};
    for (int i = 0; i < 16; ++i) {
        n.advance(8);  // one step per period at divisor 0 / s 0
        EXPECT_EQ(n.sample(), want[i]);
    }
}
TEST(noise_advance, silent_when_disabled_or_dac_off) {
    NoiseChannel n = makeChannel(0x00);
    int audible = 0;
    for (int i = 0; i < 32; ++i) {
        n.advance(8);
        audible += n.sample();
    }
    EXPECT_NE(audible, 0);  // the committed table leaves silence quickly
    n.writeNR42(0x07);  // DAC off -> channel silenced + disabled
    EXPECT_FALSE(n.dacEnabled());
    EXPECT_FALSE(n.enabled);
    EXPECT_EQ(n.sample(), 0);
}

TEST(noise_length, trigger_reload_and_expiry_disable) {
    NoiseChannel n;
    n.writeNR44(0x80);  // trigger reloads 0 -> 64 (lengthEnable bit clear)
    n.lengthEnabled = true;
    n.lengthCounter = gbaudio::kNoiseLengthMax;
    EXPECT_EQ(n.lengthCounter, gbaudio::kNoiseLengthMax);
    for (int i = 0; i < 63; ++i) n.lengthTick();
    EXPECT_TRUE(n.enabled);
    n.lengthTick();
    EXPECT_FALSE(n.enabled);
}
