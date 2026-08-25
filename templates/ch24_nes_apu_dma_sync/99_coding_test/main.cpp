#define LABSTEST_MAIN
#include "labstest.hpp"

#include "machine.hpp"

using namespace nes24sync;

// The drift-repair contract: the machine's PPU catch-up ratio must be the
// hardware ratio. With the wrong constant every raster-sensitive hash
// drifts; see CODING_TEST.md.
TEST(nes24drift, ppu_catchup_ratio_is_three_dots_per_cpu_cycle) {
    EXPECT_EQ(kPpuDotsPerCpu, 3);
}

TEST(nes24drift, frame_length_matches_the_repaired_ratio) {
    Machine m;
    // One NTSC frame is 89342 PPU dots = 29780⅔ CPU cycles at 1:3, so a
    // 60-frame run costs ~1786852 CPU cycles (±1 per frame boundary).
    for (int f = 0; f < 10; ++f) m.run_one_frame();
    int64_t cycles = int64_t(m.cpu_cycle);
    // 10 frames x ceil-ish: with ratio 3 each frame consumes 29781 or so;
    // with the broken ratio 2 it would cost ~50% more cycles per frame.
    EXPECT_TRUE(cycles > 200000 && cycles < 320000);
}
