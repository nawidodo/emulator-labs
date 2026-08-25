// Tests for 90_debug: each suite targets exactly one seeded defect.
//
// The probe stream below is also documented in DEBUGGING.md together
// with the bugged-vs-correct FNV-64 pair produced by this suite.
#define LABSTEST_MAIN
#include <cstdio>
#include <cstdint>

#include "labstest.hpp"
#include "audio_debug.hpp"

using gbapudbg::SweepEnvVoice;

namespace {

uint64_t fnv1a64(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

// The committed probe: a decaying voice swept downward in negative mode,
// sampled once per (sweep, envelope) tick pair for 96 ticks. Two bytes
// per sample: freq & 0xFF, then volume.
uint64_t probeStreamHash() {
    SweepEnvVoice v;
    v.negate = true;
    v.pace = 1;
    v.slope = 1;
    v.envPeriod = 2;
    v.initialVolume = 15;
    v.envIncrease = false;
    v.freq = 600;
    v.trigger();
    uint8_t stream[96 * 2];
    for (int i = 0; i < 96; ++i) {
        v.sweepTick();
        v.envelopeTick();
        stream[2 * i] = static_cast<uint8_t>(v.freq & 0xFF);
        stream[2 * i + 1] = static_cast<uint8_t>(v.volume);
    }
    return fnv1a64(stream, sizeof(stream));
}

}  // namespace

TEST(debug_envelope, steps_exactly_when_countdown_reaches_zero) {
    SweepEnvVoice v;
    v.envPeriod = 1;
    v.initialVolume = 9;
    v.envIncrease = false;
    v.trigger();
    const int want[5] = {8, 7, 6, 5, 4};
    for (int t = 0; t < 5; ++t) {
        v.envelopeTick();
        EXPECT_EQ(v.volume, want[t]);  // one notch per tick at period 1
    }
}

TEST(debug_envelope, period_two_steps_every_other_tick) {
    SweepEnvVoice v;
    v.envPeriod = 2;
    v.initialVolume = 12;
    v.envIncrease = false;
    v.trigger();
    const int want[6] = {12, 11, 11, 10, 10, 9};
    for (int t = 0; t < 6; ++t) {
        v.envelopeTick();
        EXPECT_EQ(v.volume, want[t]);
    }
}

TEST(debug_sweep, positive_mode_is_unaffected) {
    // Isolation guard: the seeded defect lives only in negative mode.
    SweepEnvVoice v;
    v.negate = false;
    v.pace = 1;
    v.slope = 2;
    v.freq = 1500;
    v.trigger();     // first candidate 1500 + 375 = 1875
    v.sweepTick();   // applies 1875; SECOND candidate 2343 > 2047 disables
    EXPECT_EQ(v.freq, 1875);
    EXPECT_FALSE(v.enabled);
}

TEST(debug_sweep, negative_second_update_uses_fresh_shadow) {
    SweepEnvVoice v;
    v.negate = true;
    v.pace = 1;
    v.slope = 1;
    v.freq = 600;
    v.trigger();     // first candidate 600 - 300 = 300
    EXPECT_TRUE(v.enabled);
    v.sweepTick();   // applies 300; second candidate 300 - 150 = 150
    EXPECT_EQ(v.freq, 300);
    EXPECT_EQ(v.pendingCandidate, 150);
    v.sweepTick();   // applies 150; next candidate 75
    EXPECT_EQ(v.freq, 150);
    EXPECT_EQ(v.pendingCandidate, 75);
    EXPECT_TRUE(v.enabled);
}

TEST(debug_probe, probe_stream_hash_matches_reference) {
    EXPECT_EQ(probeStreamHash(), 0xEF2E9FBF072A032BULL);
}
