#define LABSTEST_MAIN
#include "labstest.hpp"
#include "adpcm.hpp"
#include "../shared/fnv.hpp"

using spu::AdpcmDecoder;
using spu::DecodedBlock;

// Build a 16-byte block: shift/filter, flags, 28 low-first nibbles.
static std::array<uint8_t, 16> make_block(unsigned shift, unsigned filt,
                                          uint8_t flags,
                                          const unsigned* nibs) {
    std::array<uint8_t, 16> b{};
    b[0] = static_cast<uint8_t>((filt << 4) | (shift & 0xF));
    b[1] = flags;
    for (int i = 0; i < 28; ++i) {
        const int byte_idx = 2 + i / 2;
        if (i % 2 == 0)
            b[byte_idx] |= static_cast<uint8_t>(nibs[i] & 0xF);
        else
            b[byte_idx] |= static_cast<uint8_t>((nibs[i] & 0xF) << 4);
    }
    return b;
}

TEST(adpcm, sign_nibble_extends) {
    EXPECT_EQ(spu::sign_nibble(0x0), 0);
    EXPECT_EQ(spu::sign_nibble(0x7), 7);
    EXPECT_EQ(spu::sign_nibble(0x8), -8);
    EXPECT_EQ(spu::sign_nibble(0xF), -1);
}

TEST(adpcm, filter_table_exact) {
    // The published table must be exact — golden PCM depends on it.
    const int8_t expected[5][2] = {{0, 0}, {60, 0}, {115, -52},
                                   {98, -55}, {122, -60}};
    for (int f = 0; f < 5; ++f) {
        EXPECT_EQ(spu::FILTER_C[f][0], expected[f][0]);
        EXPECT_EQ(spu::FILTER_C[f][1], expected[f][1]);
    }
}

TEST(adpcm, filter0_shift0_passthrough) {
    // filter 0 = no predictor; shift 0 = raw sign-extended values.
    const unsigned nibs[28] = {1, 0xF, 2, 0xE, 8, 0x7};
    auto b = make_block(0, 0, 0x00, nibs);
    AdpcmDecoder dec;
    dec.reset();
    DecodedBlock out = dec.decode_block(b.data());
    EXPECT_EQ(out.samples[0], 1);
    EXPECT_EQ(out.samples[1], -1);
    EXPECT_EQ(out.samples[2], 2);
    EXPECT_EQ(out.samples[3], -2);
    EXPECT_EQ(out.samples[4], -8);
    EXPECT_EQ(out.samples[5], 7);
    for (int i = 6; i < 28; ++i) EXPECT_EQ(out.samples[i], 0);
}

TEST(adpcm, filter1_uses_history_low_first) {
    // Byte layout check + predictor: with c1=60, c2=0:
    //   s1 = raw1 + (hist1*60)>>6
    // Nibbles chosen so results are hand-checkable.
    // raw values: +4 then +4 again. After first sample hist=4:
    //   s2 = 4 + (4*60)>>6 = 4 + 3 = 7.
    const unsigned nibs[28] = {4, 4};
    auto b = make_block(0, 1, 0x00, nibs);
    AdpcmDecoder dec;
    dec.reset();
    DecodedBlock out = dec.decode_block(b.data());
    EXPECT_EQ(out.samples[0], 4);
    EXPECT_EQ(out.samples[1], 7);
}

TEST(adpcm, shift_scales_up) {
    // Same nibble, shift 3: value becomes 1<<3 = 8.
    const unsigned nibs[28] = {1};
    auto b = make_block(3, 0, 0x00, nibs);
    AdpcmDecoder dec;
    dec.reset();
    DecodedBlock out = dec.decode_block(b.data());
    EXPECT_EQ(out.samples[0], 8);
}

TEST(adpcm, clamps_to_int16) {
    // Large positive value through filter 0 at max legal shift.
    const unsigned nibs[28] = {7};
    auto b = make_block(14, 0, 0x00, nibs);  // 7 << 14 overflows int16
    AdpcmDecoder dec;
    dec.reset();
    DecodedBlock out = dec.decode_block(b.data());
    EXPECT_EQ(out.samples[0], 32767);
}

TEST(adpcm, history_carries_across_blocks) {
    // Block A ends with raw 4 (all earlier nibbles zero), leaving
    // hist1 = 4 at the boundary. Block B (filter 1) must predict
    // (4*60)>>6 = 3 for its first sample.
    unsigned a[28] = {0};
    a[27] = 4;
    const unsigned bb[28] = {0};
    auto ba = make_block(0, 1, 0x00, a);
    auto bc = make_block(0, 1, 0x00, bb);
    AdpcmDecoder dec;
    dec.reset();
    dec.decode_block(ba.data());
    DecodedBlock out = dec.decode_block(bc.data());
    EXPECT_EQ(out.samples[0], 3);
}

TEST(adpcm, flag_bits) {
    const unsigned nibs[28] = {0};
    auto b = make_block(0, 0, 0x07, nibs);  // loop_end | mute | loop_start
    AdpcmDecoder dec;
    dec.reset();
    DecodedBlock out = dec.decode_block(b.data());
    EXPECT_TRUE(out.flags.loop_end);
    EXPECT_TRUE(out.flags.mute);
    EXPECT_TRUE(out.flags.loop_start);
}

TEST(adpcm, reset_clears_history) {
    const unsigned a[28] = {4};
    auto b = make_block(0, 1, 0x00, a);
    AdpcmDecoder dec;
    dec.reset();
    dec.decode_block(b.data());
    dec.reset();
    // After reset the same input reproduces the same first output.
    EXPECT_EQ(dec.decode_block(b.data()).samples[0], 4);
}
