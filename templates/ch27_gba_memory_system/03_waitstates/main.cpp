#define LABSTEST_MAIN
#include "labstest.hpp"
#include "timing.hpp"

using namespace gba;

TEST(waitstates, internal_regions_run_at_one_cycle) {
    for (Region r : {Region::Bios, Region::Iwram, Region::Io,
                     Region::Palette, Region::Vram, Region::Oam}) {
        EXPECT_EQ(access_cycles(r, false), 1u);
        EXPECT_EQ(access_cycles(r, true), 1u);
    }
}

TEST(waitstates, waitstated_chips_match_the_table) {
    // N / S pairs from the LECTURE.md table.
    struct Row { Region r; unsigned n, s; };
    const Row rows[] = {{Region::Ewram, 3, 2},
                        {Region::RomWs0, 4, 2},
                        {Region::RomWs1, 3, 2},
                        {Region::RomWs2, 5, 2},
                        {Region::Sram, 5, 2}};
    for (const auto& row : rows) {
        EXPECT_EQ(access_cycles(row.r, false), row.n);
        EXPECT_EQ(access_cycles(row.r, true), row.s);
    }
}

TEST(waitstates, adjacency_makes_sequential) {
    EXPECT_TRUE(is_sequential(0x08000000, 0x08000002, 2, false));
    EXPECT_FALSE(is_sequential(0x08000000, 0x08000004, 2, false));
    EXPECT_FALSE(is_sequential(0x08000000, 0x09000002, 2, false)); // other chip? same WS0 region but not adjacent
}

TEST(waitstates, refill_forces_nonsequential) {
    // Even a perfectly adjacent fetch is N right after a branch.
    EXPECT_FALSE(is_sequential(0x080000FE, 0x08000100, 2, true));
}

TEST(waitstates, rom_burst_sequence_total) {
    // 8 halfword reads marching through WS0 after a branch:
    // first access N=4, then seven S=2 -> 4 + 14 = 18.
    std::vector<SeqStep> steps;
    uint32_t prev = 0x07FFFFFE;   // unrelated previous access
    for (int i = 0; i < 8; ++i) {
        const uint32_t addr = 0x08000100u + i * 2u;
        steps.push_back({prev, addr, 2});
        prev = addr;
    }
    EXPECT_EQ(sequence_cycles(steps), 18ull);
}

TEST(hidden, waitstates_hidden_mixed_sequence) {
    // EWRAM word, adjacent EWRAM word, jump to ROM WS1 halfword.
    std::vector<SeqStep> steps = {
        {0x0203FFFC, 0x02040000, 4},   // first access after refill: N = 3
        {0x02040000, 0x02040004, 4},   // seq: 2
        {0x02040004, 0x0A000000, 2},   // different region: N = 3
    };
    EXPECT_EQ(sequence_cycles(steps), 8ull);
}
