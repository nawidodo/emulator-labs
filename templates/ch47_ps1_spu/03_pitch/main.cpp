#define LABSTEST_MAIN
#include "labstest.hpp"
#include "pitch.hpp"

using namespace spu;

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

TEST(pitch, stepper_one_to_one_at_nominal_rate) {
    PitchStepper st;
    st.reset();
    const unsigned a0 = st.advance(0x1000);
    const uint32_t f0 = st.fraction();
    const unsigned a1 = st.advance(0x1000);
    EXPECT_EQ(a0, 1u);
    EXPECT_EQ(f0, 0u);
    EXPECT_EQ(a1, 1u);
}

TEST(pitch, stepper_half_rate_alternates) {
    PitchStepper st;
    st.reset();
    const unsigned c0 = st.advance(0x0800);   // acc = half a sample
    const uint32_t f0 = st.fraction();
    const unsigned c1 = st.advance(0x0800);   // crosses on the second tick
    const uint32_t f1 = st.fraction();
    EXPECT_EQ(c0, 0u);
    EXPECT_EQ(f0, 2048u);
    EXPECT_EQ(c1, 1u);
    EXPECT_EQ(f1, 0u);
}

TEST(pitch, stepper_double_rate_consumes_two) {
    PitchStepper st;
    st.reset();
    const unsigned n = st.advance(0x2000);
    EXPECT_EQ(n, 2u);
}
static void load_ramp(SpuRam& ram, unsigned first, int count,
                      uint8_t block_flags_last) {
    uint32_t addr = 0;
    while (count > 0) {
        unsigned nibs[28] = {0};
        int n = count < 28 ? count : 28;
        for (int i = 0; i < n; ++i) nibs[i] = (first + i) % 16;
        count -= n;
        auto b =
            make_block(0, 0, count == 0 ? block_flags_last : 0x00, nibs);
        for (int i = 0; i < 16; ++i) ram.data[addr + i] = b[i];
        addr += 16;
    }
}

TEST(resampled_voice, nominal_rate_reproduces_stream) {
    SpuRam ram;
    load_ramp(ram, 1, 56, 0x01);  // two blocks of ramp values

    VoiceRegs regs;
    regs.start_addr = 0;
    regs.pitch = 0x1000;

    ResampledVoice v;
    v.reset();
    v.key_on(regs, ram);
    // Outputs echo the stream: 1,2,...,7,-8,-7... (nibble 8 is -8).
    for (unsigned i = 0; i < 10; ++i) {
        const unsigned raw = (1 + i) % 16;
        const int16_t expect =
            raw < 8 ? static_cast<int16_t>(raw)
                    : static_cast<int16_t>(raw) - 16;
        const int16_t got = v.tick(ram);
        EXPECT_EQ(got, expect);
    }
}

TEST(resampled_voice, double_rate_skips_samples) {
    SpuRam ram;
    load_ramp(ram, 1, 56, 0x01);

    VoiceRegs regs;
    regs.start_addr = 0;
    regs.pitch = 0x2000;

    ResampledVoice v;
    v.reset();
    v.key_on(regs, ram);
    // Consuming 2 per output leaves the window on even stream samples.
    const int16_t o0 = v.tick(ram);
    const int16_t o1 = v.tick(ram);
    const int16_t o2 = v.tick(ram);
    EXPECT_EQ(o0, 2);
    EXPECT_EQ(o1, 4);
    EXPECT_EQ(o2, 6);
}

TEST(resampled_voice, inactive_after_stream_end) {
    SpuRam ram;
    load_ramp(ram, 1, 28, 0x01);  // single unlooped block

    VoiceRegs regs;
    regs.start_addr = 0;
    regs.pitch = 0x1000;

    ResampledVoice v;
    v.reset();
    v.key_on(regs, ram);
    for (int i = 0; i < 64; ++i) v.tick(ram);
    EXPECT_FALSE(v.active());
    EXPECT_EQ(v.tick(ram), 0);
}
