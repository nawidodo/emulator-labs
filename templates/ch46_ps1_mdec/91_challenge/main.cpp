#define LABSTEST_MAIN
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "labstest.hpp"
#include "mdec_core.hpp"

using namespace mchal;

TEST(mdec, dma_feed_orders_halves_correctly) {
    DmaFeed feed;
    feed.push_word(0x00100002u);   // high=0x0010 low=0x0002
    uint16_t a = 0, b = 0;
    EXPECT_TRUE(feed.read_unit(a));
    EXPECT_TRUE(feed.read_unit(b));
    EXPECT_EQ(a, 0x0010u);
    EXPECT_EQ(b, 0x0002u);
}

TEST(mdec, decodes_single_macroblock_deterministically) {
    // Build a minimal in-memory stream: six blocks of header+EOB only.
    // Header scale=16 luma table for Y blocks; chroma table for Cb/Cr.
    std::vector<unsigned char> bytes;
    auto push16 = [&](uint16_t v) {
        bytes.push_back(v >> 8);
        bytes.push_back(v & 0xFF);
    };
    for (unsigned blk = 0; blk < 6; ++blk) {
        push16(3);          // nunits: header, DC, EOB
        push16(blk == 4 || blk == 5 ? 0x8010u : 0x0010u);
        push16(0x0004u);    // DC level +4 -> value 128 (flat block)
        push16(mdec::kEndOfBlock);
    }
    if (bytes.size() % 4) bytes.push_back(0);

    const std::string path =
        (std::filesystem::temp_directory_path() / "labs_ch46_one_mb.bin")
            .string();
    EXPECT_TRUE(f != nullptr);
    if (!f) return;
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    const auto r1 = decode_stream(path);
    const auto r2 = decode_stream(path);
    EXPECT_EQ(r1.macroblocks, 1u);
    EXPECT_TRUE(r1.pixels.size() == 256);
    EXPECT_EQ(r1.pixels[0], r2.pixels[0]);       // deterministic

    // Every block decodes to one flat DC sample (IDCT of a lone DC
    // coefficient), including Cb/Cr -> fully uniform macroblock.
    // Spatial value: (128*23*23 + 64) >> 7 == 17.
    const uint16_t want = mdec::ycbcr_to_rgb15(17, 17, 17);
    bool uniform = true;
    for (int i = 0; i < 256; ++i)
        if (r1.pixels[i] != want) uniform = false;
    EXPECT_TRUE(uniform);
}
