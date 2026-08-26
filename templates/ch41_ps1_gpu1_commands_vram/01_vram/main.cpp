#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>

#include <cstdint>
#include <vector>

#include "vram.hpp"

using psx::gpu::Vram;

TEST(vram, index_wraps_x_and_y_independently) {
    // X wraps within the SAME row; there is no carry-out from X to Y
    // (PSX-SPX "Wrapping" note under the COPY commands).
    EXPECT_EQ(psx::gpu::vram_index(1023, 7), 7 * 1024 + 1023);
    EXPECT_EQ(psx::gpu::vram_index(1024, 7), 7 * 1024 + 0);
    EXPECT_EQ(psx::gpu::vram_index(1026, 7), 7 * 1024 + 2);
    EXPECT_EQ(psx::gpu::vram_index(50, 512), 0 * 1024 + 50);
    EXPECT_EQ(psx::gpu::vram_index(50, 513), 1 * 1024 + 50);
}

TEST(vram, copy_sizes_zero_means_maximum) {
    EXPECT_EQ(psx::gpu::copy_width(0), 1024u);
    EXPECT_EQ(psx::gpu::copy_height(0), 512u);
    EXPECT_EQ(psx::gpu::copy_width(1), 1u);
    EXPECT_EQ(psx::gpu::copy_width(1023), 1023u);
    EXPECT_EQ(psx::gpu::copy_width(1024), 1024u);
    EXPECT_EQ(psx::gpu::copy_width(1025), 1u);
    EXPECT_EQ(psx::gpu::copy_height(511), 511u);
    EXPECT_EQ(psx::gpu::copy_height(513), 1u);
}

TEST(vram, upload_wraps_across_right_edge_same_row) {
    // src[0]->(1022,10) src[1]->(1023,10) src[2]->(0,10) src[3]->(1,10)
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    const uint16_t src[4] = {0x111, 0x222, 0x333, 0x444};
    psx::gpu::cpu_to_vram(v, 1022, 10, 4, 1, src);
    EXPECT_EQ(v.at(1022, 10), 0x111);
    EXPECT_EQ(v.at(1023, 10), 0x222);
    EXPECT_EQ(v.at(0, 10), 0x333);   // same row, no carry into row 11
    EXPECT_EQ(v.at(1, 10), 0x444);
}

TEST(vram, upload_wraps_bottom_to_top) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    const uint16_t src[6] = {1, 2, 3, 4, 5, 6};
    psx::gpu::cpu_to_vram(v, 20, 511, 2, 3, src);
    EXPECT_EQ(v.at(20, 511), 1);
    EXPECT_EQ(v.at(21, 511), 2);
    EXPECT_EQ(v.at(20, 0), 3);  // next row wraps to top
    EXPECT_EQ(v.at(21, 0), 4);
    EXPECT_EQ(v.at(20, 1), 5);
    EXPECT_EQ(v.at(21, 1), 6);
}

TEST(vram, download_roundtrips_upload) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    std::vector<uint16_t> src(30 * 3);
    for (size_t i = 0; i < src.size(); ++i)
        src[i] = static_cast<uint16_t>(i * 211 + 7);
    psx::gpu::cpu_to_vram(v, 1010, 100, 30, 3, src);
    // Source rows cross the right edge and wrap back into the same rows.
    const std::vector<uint16_t> dst = psx::gpu::vram_to_cpu(v, 1010, 100, 30, 3);
    EXPECT_EQ(dst.size(), src.size());
    for (size_t i = 0; i < src.size(); ++i) EXPECT_EQ(dst[i], src[i]);
    // Spot-check that the wrapped tail really sits in the next VRAM row's
    // leading columns.
    EXPECT_EQ(v.at(1010 + 20, 100), src[20]);        // last column pair fits
    EXPECT_EQ(v.at(1010 + 25 - 1024, 100), src[25]); // wrapped back to col 11
}

TEST(vram, copy_is_overlap_safe_like_hardware_latches) {
    // Shifting a ramp two columns to the right must not smear: the GPU reads
    // a chunk into latches before writing it back.
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    for (int x = 0; x < 16; ++x) v.at(x, 0) = static_cast<uint16_t>(x);
    psx::gpu::vram_to_vram(v, 0, 0, 2, 0, 16, 1);
    EXPECT_EQ(v.at(2, 0), 0);   // destination got the SNAPSHOT, not the
    EXPECT_EQ(v.at(3, 0), 1);   // progressively overwritten data
    EXPECT_EQ(v.at(17, 0), 15);
}

TEST(vram, copy_wraps_source_rows_without_carry) {
    auto v_storage = std::make_unique<Vram>();
    Vram& v = *v_storage;
    // Pattern lives at the very right edge of row 5/6: an 8-wide gather at
    // sx=1020 must wrap each source row back to columns 0..3 of THAT row.
    for (int i = 0; i < 8; ++i) {
        v.at((1020 + i) % 1024, 5) = static_cast<uint16_t>(0xA0 + i);
        v.at((1020 + i) % 1024, 6) = static_cast<uint16_t>(0xB0 + i);
    }
    psx::gpu::vram_to_vram(v, 1020, 5, 100, 200, 8, 2);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(v.at(100 + i, 200), static_cast<uint16_t>(0xA0 + i));
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(v.at(100 + i, 201), static_cast<uint16_t>(0xB0 + i));
    EXPECT_EQ(v.at(108, 200), 0u);   // nothing beyond the copied block
}
