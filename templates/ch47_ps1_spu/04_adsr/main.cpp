#define LABSTEST_MAIN
#include "labstest.hpp"
#include "adsr.hpp"

using namespace spu;

TEST(adsr_rate_table, published_shape) {
    // Rates < 12 freeze.
    EXPECT_EQ(decode_rate(0).period, 0);
    EXPECT_EQ(decode_rate(11).period, 0);
    // Group 3 -> period 256, sub 0 -> step 7.
    auto r12 = decode_rate(12);
    EXPECT_EQ(r12.period, 256);
    EXPECT_EQ(r12.step, 7);
    // Fastest group: period 1; sub selects step 7..4.
    EXPECT_EQ(decode_rate(64).period, 1);
    EXPECT_EQ(decode_rate(64).step, 7);
    EXPECT_EQ(decode_rate(65).step, 6);
    EXPECT_EQ(decode_rate(66).step, 5);
    EXPECT_EQ(decode_rate(67).step, 4);
    EXPECT_EQ(decode_rate(127).step, 4);
}

TEST(adsr_unpack, register_layout) {
    // ADSR1: sl=1 -> target 0x1000; dr=2; ar=0x7F; attack exp bit set.
    uint16_t adsr1 = static_cast<uint16_t>(0x8000 | (0x7F << 8) | (2 << 4) | 1);
    // ADSR2: rr=31, release exp, sr=0x40, direction inc, sustain exp.
    uint16_t adsr2 = static_cast<uint16_t>((1 << 14) | (1 << 13) |
                                           (0x40 << 6) | 0x20 | 31);
    auto p = AdsrParams::unpack(adsr1, adsr2);
    EXPECT_TRUE(p.attack_exp);
    EXPECT_EQ(p.ar, 0x7F);
    EXPECT_EQ(p.dr, 2);
    EXPECT_EQ(p.sustain_level, 0x1000);
    EXPECT_EQ(p.rr, 31);
    EXPECT_TRUE(p.release_exp);
    EXPECT_EQ(p.sr, 0x40);
    EXPECT_TRUE(p.sustain_inc);
    EXPECT_TRUE(p.sustain_exp);
}

TEST(adsr_attack, linear_reaches_max_then_decays) {
    AdsrParams p;
    p.ar = 64;   // linear, update every sample, step 7
    p.dr = 15;   // exponential decay
    p.sustain_level = 0x800;
    Adsr env;
    env.key_on(p);
    EXPECT_EQ(env.phase(), AdsrPhase::Attack);
    for (int i = 1; i <= 10; ++i) {
        const int lvl = env.tick();
        EXPECT_EQ(lvl, i * 7);
    }
    // 32767 == 7 * 4681: the transition happens exactly here.
    for (int i = 11; i < 4681; ++i) env.tick();
    EXPECT_EQ(env.phase(), AdsrPhase::Attack);
    const int top = env.tick();
    EXPECT_EQ(top, kAdsrMax);
    EXPECT_EQ(env.phase(), AdsrPhase::Decay);
}

TEST(adsr_decay, exponential_steps) {
    AdsrParams p;
    p.ar = 64;
    p.dr = 15;               // period 256, step 7
    p.sustain_level = 0x800; // 2048
    Adsr env;
    env.key_on(p);
    while (env.phase() != AdsrPhase::Decay) env.tick();
    // 255 more decay samples do nothing (period 256).
    bool held = true;
    for (int i = 0; i < 255; ++i)
        if (env.tick() != kAdsrMax) held = false;
    EXPECT_TRUE(held);
    // Update #1 at rate 15 (step 7-(15&3)=4):
    // 32767 - (32767*4)>>6 = 32767 - 2047 = 30720.
    const int d1 = env.tick();
    EXPECT_EQ(d1, 30720);
    // Eventually crosses into sustain.
    for (int i = 0; i < 20000 && env.phase() != AdsrPhase::Sustain; ++i)
        env.tick();
    EXPECT_EQ(env.phase(), AdsrPhase::Sustain);
    EXPECT_TRUE(env.level() <= p.sustain_level);
}

TEST(adsr_sustain, holds_until_key_off) {
    AdsrParams p;
    p.ar = 64;
    p.dr = 15;
    p.sr = 64;               // linear sustain, step 7 every sample
    p.sustain_inc = true;
    p.sustain_level = 0x800; // decay ends below any rising sustain
    p.rr = 64;
    p.release_exp = false;
    Adsr env;
    env.key_on(p);
    while (env.phase() != AdsrPhase::Sustain) env.tick();
    const int start = env.level();
    EXPECT_TRUE(env.tick() > start);  // rising again in sustain
    env.key_off();
    EXPECT_EQ(env.phase(), AdsrPhase::Release);
    while (env.phase() != AdsrPhase::Off) env.tick();
    EXPECT_EQ(env.level(), 0);
    EXPECT_EQ(env.tick(), 0);      // stays silent
}

TEST(adsr_release, exponential_falloff_reaches_zero) {
    AdsrParams p;
    p.ar = 64;
    p.rr = 64;
    p.release_exp = false;
    p.dr = 15;
    p.sustain_level = 0x800;
    Adsr env;
    env.key_on(p);
    while (env.phase() != AdsrPhase::Decay) env.tick();
    env.key_off();
    // Linear step 7 per sample from ~32767: done within ~4700 samples.
    for (int i = 0; i < 6000 && env.phase() != AdsrPhase::Off; ++i)
        env.tick();
    EXPECT_EQ(env.phase(), AdsrPhase::Off);
}

TEST(adsr_frozen, low_rates_never_advance) {
    AdsrParams p;
    p.ar = 0;  // frozen
    Adsr env;
    env.key_on(p);
    for (int i = 0; i < 1000; ++i) EXPECT_EQ(env.tick(), 0);
    EXPECT_EQ(env.phase(), AdsrPhase::Attack);
}
