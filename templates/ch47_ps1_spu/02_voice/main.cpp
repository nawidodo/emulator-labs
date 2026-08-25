#define LABSTEST_MAIN
#include "labstest.hpp"
#include "voice.hpp"
#include "../shared/fnv.hpp"

using spu::SpuRam;
using spu::Voice;
using spu::VoiceRegs;

// Assemble one ADPCM block (shift/filter/flags + low-first nibbles).
static std::array<uint8_t, 16> make_block(unsigned shift, unsigned filt,
                                          uint8_t flags,
                                          const unsigned* nibs) {
    std::array<uint8_t, 16> b{};
    b[0] = static_cast<uint8_t>((filt << 4) | (shift & 0xF));
    b[1] = flags;
    for (int i = 0; i < 28; ++i) {
        if (i % 2 == 0)
            b[2 + i / 2] |= static_cast<uint8_t>(nibs[i] & 0xF);
        else
            b[2 + i / 2] |= static_cast<uint8_t>((nibs[i] & 0xF) << 4);
    }
    return b;
}

static VoiceRegs regs_at(uint16_t start_addr_word) {
    VoiceRegs r;
    r.start_addr = start_addr_word;
    r.vol_left = r.vol_right = 0x4000;
    return r;
}

TEST(voice, silent_when_never_keyed) {
    SpuRam ram;
    Voice v;
    v.reset();
    EXPECT_EQ(v.tick(ram), 0);
    EXPECT_FALSE(v.active());
}

TEST(voice, key_on_plays_stream_low_first) {
    SpuRam ram;
    // Block at byte address 0x100 (word 0x20): raw values 1,-1,2,-2...
    const unsigned nibs[28] = {1, 0xF, 2, 0xE};
    auto b = make_block(0, 0, 0x00, nibs);
    for (int i = 0; i < 16; ++i) ram.data[0x100 + i] = b[i];

    Voice v;
    v.reset();
    v.key_on(regs_at(0x100 >> 3));
    EXPECT_TRUE(v.active());
    const int16_t s0 = v.tick(ram);
    const int16_t s1 = v.tick(ram);
    const int16_t s2 = v.tick(ram);
    const int16_t s3 = v.tick(ram);
    EXPECT_EQ(s0, 1);
    EXPECT_EQ(s1, -1);
    EXPECT_EQ(s2, 2);
    EXPECT_EQ(s3, -2);
    EXPECT_EQ(v.current_addr(), 0x110);  // advanced past the first block
}

TEST(voice, ends_after_unlooped_block) {
    SpuRam ram;
    const unsigned nibs[28] = {5};
    auto b = make_block(0, 0, 0x01, nibs);  // loop_end, no loop_start
    for (int i = 0; i < 16; ++i) ram.data[i] = b[i];

    Voice v;
    v.reset();
    v.key_on(regs_at(0));
    const int16_t first = v.tick(ram);
    EXPECT_EQ(first, 5);
    // Burn the rest of the block.
    int zeros = 0;
    for (int i = 0; i < 27; ++i) {
        const int16_t s = v.tick(ram);
        if (s == 0) ++zeros;
    }
    EXPECT_EQ(zeros, 27);
    // The next tick notices loop_end without a loop address and stops.
    const int16_t tail = v.tick(ram);
    EXPECT_EQ(tail, 0);
    EXPECT_FALSE(v.active());
}

TEST(voice, loops_back_to_loop_address) {
    SpuRam ram;
    const unsigned nibs_a[28] = {3};   // loop start marker
    const unsigned nibs_b[28] = {6};   // loop end -> jump back to A
    auto ba = make_block(0, 0, 0x04, nibs_a);
    auto bb = make_block(0, 0, 0x05, nibs_b);  // end | loop_start? no: 0x05=end+mute!
    // Careful: bit1 is mute. Use 0x01 only.
    bb[1] = 0x01;
    for (int i = 0; i < 16; ++i) ram.data[0x40 + i] = ba[i];
    for (int i = 0; i < 16; ++i) ram.data[0x50 + i] = bb[i];

    Voice v;
    v.reset();
    v.key_on(regs_at(0x40 >> 3));
    const int16_t sa = v.tick(ram);          // from block A
    EXPECT_EQ(sa, 3);
    // The first block carried loop_start, recorded during its fetch.
    EXPECT_TRUE(v.loop_addr().has_value());
    for (int i = 0; i < 27; ++i) v.tick(ram);
    EXPECT_EQ(v.current_addr(), 0x50);
    const int16_t sb = v.tick(ram);          // first sample of block B
    EXPECT_EQ(sb, 6);
    for (int i = 0; i < 27; ++i) v.tick(ram);
    EXPECT_TRUE(v.active());
    const int16_t sc = v.tick(ram);          // jumped back into block A
    EXPECT_EQ(sc, 3);
}

TEST(voice, mute_flag_silences_and_ends) {
    SpuRam ram;
    const unsigned nibs[28] = {9};
    auto b = make_block(0, 0, 0x02, nibs);  // mute
    for (int i = 0; i < 16; ++i) ram.data[i] = b[i];

    Voice v;
    v.reset();
    v.key_on(regs_at(0));
    const int16_t s = v.tick(ram);
    EXPECT_EQ(s, 0);
    EXPECT_FALSE(v.active());
}

TEST(voice, reset_clears_loop_state) {
    SpuRam ram;
    const unsigned nibs[28] = {1};
    auto b = make_block(0, 0, 0x04, nibs);  // loop_start
    for (int i = 0; i < 16; ++i) ram.data[i] = b[i];

    Voice v;
    v.reset();
    v.key_on(regs_at(0));
    v.tick(ram);  // fetches the block and records its loop address
    EXPECT_TRUE(v.loop_addr().has_value());
    v.reset();
    EXPECT_FALSE(v.loop_addr().has_value());
    EXPECT_FALSE(v.active());
}
