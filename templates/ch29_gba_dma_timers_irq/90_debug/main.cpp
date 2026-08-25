#define LABSTEST_MAIN
#include "labstest.hpp"
#include "race.hpp"

using namespace gba;

// Real-hardware expectations; the seeded bugs fail these.
TEST(hblank, fires_mid_scanline) {
    EXPECT_EQ(hblank_dma_cycle(0), 960u);
    EXPECT_EQ(hblank_dma_cycle(1), 1232u + 960u);
    EXPECT_EQ(hblank_dma_cycle(160), u64(160) * 1232 + 960);
    // Consecutive lines are exactly one scanline apart.
    EXPECT_EQ(hblank_dma_cycle(5) - hblank_dma_cycle(4), 1232u);
}

TEST(timer, period_and_reload_exact) {
    // Reload 0xFFFB -> period 5 ticks.
    EXPECT_EQ(timer_period_ticks(0xFFFB), 5u);
    EXPECT_EQ(timer_period_ticks(0xFFFF), 1u);
    EXPECT_EQ(timer_period_ticks(0), 0x10000u);   // full-range period
    // After overflow the counter restarts AT the reload value.
    EXPECT_EQ(timer_post_overflow_counter(0xFFFB), 0xFFFB);
    EXPECT_EQ(timer_post_overflow_counter(u16(0x8000)), 0x8000);
}
