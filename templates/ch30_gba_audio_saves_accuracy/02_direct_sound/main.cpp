#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "dsound.hpp"

using namespace gba;

TEST(fifo, push_pop_order_and_wrap) {
    SoundFifo f;
    f.reset();
    for (int i = 0; i < 40; ++i) f.push(u8(i));  // wraps: oldest overwritten
    EXPECT_EQ(f.used, kFifoSize);
    // Oldest surviving byte is 8 (40 pushed, 32 kept).
    EXPECT_EQ(f.pop(), s8(8));
    EXPECT_EQ(f.pop(), s8(9));
}

TEST(fifo, empty_pops_hold_last_sample) {
    SoundFifo f;
    f.reset();
    f.push(0x7F);
    EXPECT_EQ(f.pop(), s8(0x7F));
    // Empty now: repeats the last delivered byte.
    EXPECT_EQ(f.pop(), s8(0x7F));
    EXPECT_EQ(f.pop(), s8(0x7F));
}

TEST(volume, shift_codes) {
    s8 full = 100;
    EXPECT_EQ(apply_dsound_volume(full, 2), 100);
    EXPECT_EQ(apply_dsound_volume(full, 1), 50);
    EXPECT_EQ(apply_dsound_volume(full, 0), 25);
    EXPECT_EQ(apply_dsound_volume(full, 3), 0);  // prohibited -> mute
}

TEST(bias, offset_and_quantization) {
    // No resolution bits, bias 512: mixed 0 -> 512.
    EXPECT_EQ(bias_quantize(0, 512), 512u);
    // Clamps at both rails.
    EXPECT_EQ(bias_quantize(-20000, 512), 0u);
    EXPECT_EQ(bias_quantize(20000, 512), 1023u);
    // Resolution 1 (bits14-15 = 01): low bit dropped.
    u16 reg = u16(512 | (1 << 14));
    EXPECT_EQ(bias_quantize(3, reg), 514u);   // 515 -> drop low bit
    EXPECT_EQ(bias_quantize(4, reg), 516u);   // 516 stays
}

namespace {
void fill(SoundFifo& a, SoundFifo& b) {
    a.reset();
    b.reset();
    for (int i = 0; i < 32; ++i) {
        a.push(u8(64 + i));
        b.push(u8(16 + i));
    }
}
}  // namespace

TEST(pcm, deterministic_and_values_sensible) {
    std::vector<u16> pcm;
    SoundFifo a, b;
    fill(a, b);
    u64 h1 = render_pcm(a, b, 10, 2, 1, 512, 100, 40, pcm);
    fill(a, b);
    u64 h2 = render_pcm(a, b, 10, 2, 1, 512, 100, 40, pcm);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(pcm.size(), 40u);

    // Fresh state: A byte 64 at 100%, B byte 16 at 50% (=8) doubled,
    // plus PSG level and bias.
    fill(a, b);
    render_pcm(a, b, 10, 2, 1, 512, 100, 40, pcm);
    EXPECT_EQ(pcm[0], u16(10 + 64 + 8 * 2 + 512));
}
