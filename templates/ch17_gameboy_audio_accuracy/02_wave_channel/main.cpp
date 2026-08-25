// Tests for exercise 02: wave channel playback.
//
// The distinctive wave RAM pattern committed as
// tests/public/ch17_gameboy_audio_accuracy/fixtures/wave_ram_pattern.bin
// is duplicated byte-for-byte here so the suite stays self-contained
// (see provenance.md: 0x0F,0x1E,0x2D,... nibble ramp).
#define LABSTEST_MAIN
#include <cstdio>

#include "labstest.hpp"
#include "wave.hpp"

using gbaudio::WaveChannel;

namespace {

const uint8_t kWaveRam[16] = {0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A,
                              0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4,
                              0xC3, 0xD2, 0xE1, 0xF0};

WaveChannel makePlayer(uint16_t freq) {
    WaveChannel w;
    for (int i = 0; i < 16; ++i) w.writeWaveRam(i, kWaveRam[i]);
    w.writeNR30(0x80);   // DAC on
    w.writeNR32(0x20);   // code 1 = 100%
    w.writeNR33(static_cast<uint8_t>(freq & 0xFF));
    w.writeNR34(static_cast<uint8_t>(0x80 | (freq >> 8)));
    return w;
}

}  // namespace

TEST(wave_nibble, even_high_odd_low) {
    WaveChannel w;
    for (int i = 0; i < 16; ++i) w.writeWaveRam(i, kWaveRam[i]);
    EXPECT_EQ(w.sampleNibble(0), 0x0);   // byte 0x0F high
    EXPECT_EQ(w.sampleNibble(1), 0xF);   // byte 0x0F low
    EXPECT_EQ(w.sampleNibble(2), 0x1);
    EXPECT_EQ(w.sampleNibble(3), 0xE);
    EXPECT_EQ(w.sampleNibble(30), 0xF);  // byte 15 = 0xF0 high
    EXPECT_EQ(w.sampleNibble(31), 0x0);
}

TEST(wave_trigger, resets_position_and_timer) {
    WaveChannel w = makePlayer(2047);  // period 2 T-cycles per position
    w.advance(64);                     // walks positions while playing
    EXPECT_EQ(w.position, 0);          // wrapped all the way around
    w.position = 17;
    w.writeNR34(0x80 | 0x07);          // re-trigger same freq
    EXPECT_EQ(w.enabled, true);
    EXPECT_EQ(w.position, 0);          // trigger rewinds to sample 0
}

TEST(wave_advance, steps_at_programmed_rate_and_wraps) {
    // freq 100 -> period (2048-100)*2 = 3896 T-cycles per position.
    WaveChannel w = makePlayer(100);
    w.advance(3896);
    EXPECT_EQ(w.position, 1);
    w.advance(3895);
    EXPECT_EQ(w.position, 1);  // one cycle short of the next step
    w.advance(1);
    EXPECT_EQ(w.position, 2);
    // Long run wraps modulo 32 deterministically.
    for (int i = 0; i < 30; ++i) w.advance(3896);
    EXPECT_EQ(w.position, 0);
}

TEST(wave_volume, codes_scale_the_nibble) {
    const int raw = 0xD;  // position 26: byte 13 = 0xD2 high nibble
    const uint8_t codes[4] = {0x00, 0x20, 0x40, 0x60};
    const int want[4] = {0, raw, raw >> 1, raw >> 2};
    for (int c = 0; c < 4; ++c) {
        WaveChannel w = makePlayer(100);
        w.writeNR32(codes[c]);
        w.position = 26;
        EXPECT_EQ(w.sample(), want[c]);
    }
}

TEST(wave_gate, dac_off_or_disabled_outputs_zero) {
    WaveChannel w = makePlayer(100);
    EXPECT_EQ(w.sample(), w.sampleNibble(0));  // audible at pos 0
    w.writeNR30(0x00);                         // DAC off disables ch3
    EXPECT_FALSE(w.dacEnabled());
    EXPECT_FALSE(w.enabled);
    EXPECT_EQ(w.sample(), 0);
    w.writeNR30(0x80);  // DAC back on, but still not triggered
    EXPECT_EQ(w.sample(), 0);
    w.trigger();
    w.advance(3896);           // one period -> position 1 (low nibble 0xF)
    EXPECT_EQ(w.sample(), 15);
}

TEST(wave_fixture, pattern_bytes_match_committed_fixture) {
    // Mirrors tests/public/ch17_gameboy_audio_accuracy/fixtures/
    // wave_ram_pattern.bin (provenance.md documents the generator).
    const uint8_t fixture[16] = {0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A,
                                 0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4,
                                 0xC3, 0xD2, 0xE1, 0xF0};
    for (int i = 0; i < 16; ++i) EXPECT_EQ(kWaveRam[i], fixture[i]);
}
