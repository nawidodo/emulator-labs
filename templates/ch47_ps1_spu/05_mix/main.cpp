#define LABSTEST_MAIN
#include "labstest.hpp"
#include "spu.hpp"
#include "../shared/fnv.hpp"
#include <cstddef>

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

TEST(spu_regs, roundtrip_voice_fields) {
    Spu spu;
    spu.reset();
    spu.write(0x044, 0x1234);   // voice 4 pitch
    EXPECT_EQ(spu.read(0x044), 0x1234);
    spu.write(0x026, 0xBEEF);   // voice 2 vol_right
    EXPECT_EQ(spu.read(0x026), 0xBEEF);
    spu.write(0x180, 0x4000);   // main volume L
    EXPECT_EQ(spu.read(0x180), 0x4000);
}

TEST(spu_key_events, register_bitmask_starts_voice) {
    Spu spu;
    spu.reset();
    // Loud constant block at word address 0x200 (byte 0x1000).
    const unsigned nibs[28] = {7};
    auto b = make_block(14, 0, 0x01, nibs);
    std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(b.data()), 16);
    const size_t done = spu.dma_write(0x1000, bytes);
    EXPECT_EQ(done, size_t(16));

    spu.write(0x000, 0x4000);   // v0 vol L
    spu.write(0x002, 0x4000);   // v0 vol R
    spu.write(0x004, 0x1000);   // pitch 1.0x
    spu.write(0x006, 0x0200);   // start addr >>3
    spu.write(0x008, 0x0000 | (64 << 8));  // ADSR1: linear fast attack
    spu.write(0x00A, 64);                  // ADSR2: linear fast release
    spu.write(0x180, 0x4000);   // main volume L (unity)
    spu.write(0x182, 0x4000);   // main volume R
    spu.write(0x1C0, 0x0001);   // KEY_ON voice 0

    std::vector<int16_t> pcm;
    spu.render(1, pcm);
    EXPECT_EQ(pcm.size(), size_t(2));
    // env=7 after one tick; s=32767 -> voiced = 6; unity volumes keep it.
    EXPECT_EQ(pcm[0], 6);
    EXPECT_EQ(pcm[1], 6);

    spu.render(10, pcm);
    EXPECT_EQ(pcm.size(), size_t(22));
}

TEST(spu_dma, wraps_at_ram_end_and_fills) {
    Spu spu;
    spu.reset();
    const uint8_t chunk[4] = {1, 2, 3, 4};
    const size_t done = spu.dma_write(kSpuRamSize - 2,
                                      std::span<const uint8_t>(chunk, 4));
    EXPECT_EQ(done, size_t(2));
    EXPECT_EQ(spu.ram().data[kSpuRamSize - 1], 2);
}

TEST(spu_irq9, dma_match_raises_flag_when_enabled) {
    Spu spu;
    spu.reset();
    const uint8_t chunk[4] = {0, 0, 0, 0};
    spu.write(0x1B4, 0x100);          // compare addr word 0x100 = byte 0x800
    spu.dma_write(0x7FE, std::span<const uint8_t>(chunk, 4));
    EXPECT_FALSE(spu.irq_flag());     // control.6 not set yet
    spu.write(0x1D8, 0x40);           // IRQ9 enable
    spu.dma_write(0x7FE, std::span<const uint8_t>(chunk, 4));
    EXPECT_TRUE(spu.irq_flag());
    spu.ack_irq();
    EXPECT_FALSE(spu.irq_flag());
}

TEST(spu_cd_input, mixed_through_cd_and_main_volume) {
    Spu spu;
    spu.reset();
    spu.set_cd_input(4096, -4096);
    spu.write(0x198, 0x2000);   // CD L half
    spu.write(0x19A, 0x2000);   // CD R half
    spu.write(0x180, 0x4000);   // main x1
    spu.write(0x182, 0x4000);
    std::vector<int16_t> pcm;
    spu.render(1, pcm);
    EXPECT_EQ(pcm[0], 2048);
    EXPECT_EQ(pcm[1], -2048);
}

TEST(spu_reverb, registers_accepted_but_bypassed) {
    Spu spu;
    spu.reset();
    // Reverb page writes must not corrupt state or audio.
    spu.write(0x1E0, 0xFFFF);
    spu.write(0x1FE, 0x1234);
    spu.set_cd_input(1000, 1000);
    spu.write(0x198, 0x4000);
    spu.write(0x19A, 0x4000);
    spu.write(0x180, 0x4000);
    spu.write(0x182, 0x4000);
    std::vector<int16_t> pcm;
    spu.render(1, pcm);
    EXPECT_EQ(pcm[0], 1000);
    EXPECT_EQ(pcm[1], 1000);
}
