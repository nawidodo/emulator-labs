#define LABSTEST_MAIN
#include "labstest.hpp"
#include "transfer.hpp"

using ps1::ChannelRegs;
using ps1::Ram;
using ps1::TransferResult;
using ps1::VectorEndpoint;

TEST(transfer, total_words_multiplies_bcr_fields) {
    ChannelRegs r;
    r.bcr = (16u << 16) | 16u;  // 16 blocks x 16 words
    EXPECT_EQ(ps1::total_words(r), 256u);
    r.bcr = (1u << 16) | 320u;  // single long burst
    EXPECT_EQ(ps1::total_words(r), 320u);
}

TEST(transfer, burst_ram_to_device_in_order) {
    Ram ram;
    ram.fill_pattern(0x1000, 100, 8);
    ChannelRegs r;
    r.madr = 0x1000;
    r.bcr = (2u << 16) | 4u;
    r.chcr = 0x00000100u;  // sync=burst, dir bit clear => RAM -> device
    VectorEndpoint dev;
    const TransferResult res = ps1::run_burst(ram, dev, r);
    EXPECT_EQ(res.words, 8u);
    EXPECT_EQ(res.cycles, 16u);  // words + startup
    EXPECT_TRUE(dev.sink().size() == 8);
    for (uint32_t i = 0; i < 8; ++i)
        EXPECT_EQ(dev.sink()[i], 100 + i);  // ascending addresses
}

TEST(transfer, burst_device_to_ram_direction) {
    Ram ram;
    std::vector<uint32_t> src{7, 8, 9};
    ChannelRegs r;
    r.madr = 0x2000;
    r.bcr = (1u << 16) | 3u;
    r.chcr = 0x00000101u;  // dir bit set: device -> RAM
    VectorEndpoint dev(src);
    const TransferResult res = ps1::run_burst(ram, dev, r);
    EXPECT_EQ(res.words, 3u);
    EXPECT_EQ(ram.read(0x2000), 7u);
    EXPECT_EQ(ram.read(0x2004), 8u);
    EXPECT_EQ(ram.read(0x2008), 9u);
}

TEST(transfer, window_decode) {
    EXPECT_EQ(ps1::dma_window_words(0), 0u);   // disabled
    EXPECT_EQ(ps1::dma_window_words(1), 8u);   // (n+1)*4
    EXPECT_EQ(ps1::dma_window_words(7), 32u);
    EXPECT_EQ(ps1::cpu_window_cycles(0), 0u);  // disabled
    EXPECT_EQ(ps1::cpu_window_cycles(2), 24u); // (n+1)*8
}

TEST(transfer, slice_matches_burst_content_and_counts_cycles) {
    ChannelRegs r;
    r.madr = 0x3000;
    r.bcr = (5u << 16) | 2u;  // 10 words total
    r.chcr = 0x00000100u;

    // Reference: plain burst.
    Ram ref_ram;
    ref_ram.fill_pattern(0x3000, 500, 10);
    VectorEndpoint ref_ep;
    ps1::run_burst(ref_ram, ref_ep, r);

    // Slice: 8-word DMA windows, 24-cycle CPU windows.
    Ram slice_ram;
    slice_ram.fill_pattern(0x3000, 500, 10);
    VectorEndpoint slice_ep;
    const TransferResult res = ps1::run_slice(slice_ram, slice_ep, r, 1, 2);
    // chunks: 8 then 2; cycles = 8 + 24 + 2 = 34.
    EXPECT_EQ(res.words, 10u);
    EXPECT_EQ(res.cycles, 34u);

    EXPECT_TRUE(slice_ep.sink().size() == ref_ep.sink().size());
    for (size_t i = 0; i < ref_ep.sink().size(); ++i)
        EXPECT_EQ(slice_ep.sink()[i], ref_ep.sink()[i]);
}

TEST(transfer, slice_without_chopping_is_single_chunk) {
    Ram ram;
    ram.fill_pattern(0x400, 1, 6);
    ChannelRegs r;
    r.madr = 0x400;
    r.bcr = (1u << 16) | 6u;
    r.chcr = 0x00000100u;
    VectorEndpoint dev;
    const TransferResult res = ps1::run_slice(ram, dev, r, 0, 0);
    EXPECT_EQ(res.words, 6u);
    EXPECT_EQ(res.cycles, 6u);  // no CPU windows inserted
}
