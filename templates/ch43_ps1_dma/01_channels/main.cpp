#define LABSTEST_MAIN
#include "labstest.hpp"
#include "dma.hpp"

using ps1::DmaController;

namespace {
constexpr uint32_t kDicrPort = 0x1F8010F4u;
}  // namespace

TEST(dma, dpcr_priority_fields) {
    DmaController d;
    d.set_dpcr(0x07654321u);  // hardware reset default
    EXPECT_EQ(d.priority(0), 1u);
    EXPECT_EQ(d.priority(2), 3u);
    EXPECT_EQ(d.priority(6), 7u);
    d.set_dpcr(0x00000000u);
    EXPECT_EQ(d.priority(2), 0u);
}

TEST(dma, channel_enable_gating) {
    DmaController d;
    // GPU channel (2): needs DPCR nibble enable + CHCR start.
    d.set_dpcr(0x00000000u);
    d.channel(2).chcr = 1u << 24;  // start busy, burst mode
    EXPECT_FALSE(d.channel_enabled(2));  // DPCR enable missing
    d.set_dpcr(0x00000F00u);             // nibble 2: enable + priority 7
    EXPECT_TRUE(d.channel_enabled(2));
    // Force trigger substitutes for the software start bit.
    d.channel(2).chcr = 1u << 28;
    EXPECT_TRUE(d.channel_enabled(2));
    // Neither bit set -> not runnable.
    d.channel(2).chcr = 0;
    EXPECT_FALSE(d.channel_enabled(2));
}

TEST(dma, reg_decode_roundtrip) {
    DmaController d;
    d.write_reg(0x1F8010A8u, 0xDEADBEEFu);  // GPU CHCR (channel 2)
    EXPECT_EQ(d.read_reg(0x1F8010A8u), 0xDEADBEEFu);
    EXPECT_EQ(d.channel(2).chcr, 0xDEADBEEFu);
    d.write_reg(0x1F8010A4u, 0x00010004u);  // GPU BCR
    EXPECT_EQ(d.read_reg(0x1F8010A4u), 0x00010004u);

    // Unmapped slot inside a channel block reads open bus.
    EXPECT_EQ(d.read_reg(0x1F8010ACu), 0xFFFFFFFFu);
    // Unknown high address too.
    EXPECT_EQ(d.read_reg(0x1F8010FCu), 0xFFFFFFFFu);
}

TEST(dma, dicr_completion_flags) {
    DmaController d;
    d.set_completion_flag(2);  // GPU finished
    d.acknowledge(0);          // nothing cleared
    EXPECT_TRUE((d.dicr() >> 26) & 1u);
    d.acknowledge(1u << 26);   // write-one-to-clear
    EXPECT_FALSE((d.dicr() >> 26) & 1u);
}

TEST(dma, dicr_master_gates_irq) {
    DmaController d;
    d.set_completion_flag(3);  // CDROM done (channels 0..5 have IRQ bits;
                               // OTC does not raise DMA interrupts on real
                               // hardware either).
    d.write_reg(kDicrPort, (1u << (16 + 3)) | (1u << 30));  // en + master
    EXPECT_TRUE(d.irq_active());
    d.write_reg(kDicrPort, 1u << (16 + 3));  // master dropped
    EXPECT_FALSE(d.irq_active());
    // Force bit (bit 3 for channel 3) raises through the enables too.
    d.write_reg(kDicrPort, (1u << (16 + 3)) | (1u << 3) | (1u << 30));
    EXPECT_TRUE(d.irq_active());
}
