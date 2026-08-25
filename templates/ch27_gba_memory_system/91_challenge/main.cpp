#define LABSTEST_MAIN
#include "labstest.hpp"
#include "timing_calc.hpp"

using namespace gba;

TEST(calc, burst_totals_match_the_tables) {
    // WS0 halfwords after refill: 4 + 7*2 = 18.
    EXPECT_EQ(burst_total(Region::RomWs0, 0x08000000u, 8, 2), 18ull);
    // EWRAM words: 3 + 3*2 = 9.
    EXPECT_EQ(burst_total(Region::Ewram, 0x02000000u, 4, 4), 9ull);
    // Internal regions: every access is 1.
    EXPECT_EQ(burst_total(Region::Iwram, 0x03000000u, 16, 2), 16ull);
}

TEST(calc, burst_of_one_is_nonsequential) {
    EXPECT_EQ(burst_total(Region::RomWs2, 0x0C000000u, 1, 4), 5ull);
    EXPECT_EQ(burst_total(Region::Sram, 0x0E000000u, 1, 1), 5ull);
}

TEST(calc, fastest_chip_depends_on_burst_length) {
    // All three chips share S=2, so the lowest N (WS1) always wins.
    EXPECT_EQ(fastest_rom_chip(16, 2), Region::RomWs1);
    EXPECT_EQ(fastest_rom_chip(1, 2), Region::RomWs1);
    // WS2's higher N makes its burst exactly 2 cycles slower.
    EXPECT_EQ(burst_total(Region::RomWs2, 0x0C000000u, 16, 2),
              burst_total(Region::RomWs1, 0x0A000000u, 16, 2) + 2ull);
}

TEST(hidden, calc_hidden_sequence_identity) {
    // Calculator totals must equal the reference bus accounting.
    std::vector<SeqStep> steps;
    uint32_t prev = 0x07FFFFFE;
    for (int i = 0; i < 6; ++i) {
        const uint32_t addr = 0x08800000u + i * 2u;   // still WS0 mirror? no:
        steps.push_back({prev, addr, 2});             // 09-range: WS0 too
        prev = addr;
    }
    EXPECT_EQ(sequence_cycles(steps),
              burst_total(Region::RomWs0, 0x08800000u, 6, 2));
}
