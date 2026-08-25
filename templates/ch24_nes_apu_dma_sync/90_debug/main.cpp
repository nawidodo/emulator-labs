#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "dbg_dma.hpp"

using nes24dbg::run_oam_dma;

namespace {
void fill(uint8_t* p) {
    for (int i = 0; i < 256; ++i) p[i] = uint8_t(i * 7 + 1);
}
}  // namespace

// The DMA cycle bill is the contract everything downstream trusts.
TEST(nes24dbg, dma_bill_is_513_from_even_start) {
    uint8_t src[256], dst[256];
    fill(src);
    EXPECT_EQ(run_oam_dma(false, src, dst), 513);
}

TEST(nes24dbg, dma_bill_is_514_from_odd_start) {
    uint8_t src[256], dst[256];
    fill(src);
    EXPECT_EQ(run_oam_dma(true, src, dst), 514);
}

TEST(nes24dbg, all_256_bytes_transfer_despite_the_bill) {
    uint8_t src[256], dst[256] = {};
    fill(src);
    run_oam_dma(false, src, dst);
    for (int i = 0; i < 256; ++i) EXPECT_EQ(dst[i], src[i]);
}

// Consequence test: a raster split timed right after $4014 lands one CPU
// cycle (three PPU dots) early while the defect is present.
TEST(nes24dbg, raster_shift_after_dma_matches_hardware_accounting) {
    uint8_t src[256], dst[256];
    fill(src);
    int bill = run_oam_dma(true, src, dst);
    int ppu_dots_shifted = bill * 3;
    EXPECT_EQ(ppu_dots_shifted, 514 * 3);   // odd start: full 514 cycles
}
