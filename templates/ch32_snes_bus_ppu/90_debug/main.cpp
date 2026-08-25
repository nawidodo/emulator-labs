#define LABSTEST_MAIN
#include "labstest.hpp"

#include "compose.hpp"

using namespace snesbus;

namespace {

constexpr PixelCandidate cand(uint8_t color, uint8_t layer, uint8_t prio) {
    return PixelCandidate{color, 0, layer, prio};
}

}  // namespace

TEST(compose, bg1_outranks_lower_layers) {
    const PixelCandidate c[] = {cand(9, 2, 1), cand(7, 1, 1), cand(5, 0, 0)};
    // BG1 (layer 0) wins even with priority clear while BG2/BG3 assert theirs.
    EXPECT_EQ(compose(c), 2);
}

TEST(compose, priority_breaks_ties_within_layer) {
    const PixelCandidate c[] = {cand(6, 0, 0), cand(8, 0, 1)};
    EXPECT_EQ(compose(c), 1);
}

TEST(compose, bg2_outranks_bg3_regardless_of_priority) {
    const PixelCandidate c[] = {cand(4, 2, 1), cand(3, 1, 0)};
    EXPECT_EQ(compose(c), 1);
}

TEST(compose, single_candidate_and_empty) {
    const PixelCandidate one[] = {cand(2, 1, 0)};
    EXPECT_EQ(compose(one), 0);
    EXPECT_EQ(compose(std::span<const PixelCandidate>()), -1);
}

TEST(compose, key_ordering_total_order) {
    // Full ordering: (L0,P1) < (L0,P0) is FALSE — L0P0 key=0 beats L0P1? No:
    // keys are 0 and 1, so P0 sorts first when alone. Enumerate the lattice.
    struct Case {
        PixelCandidate a;
        PixelCandidate b;
        int winner;
    };
    const Case cases[] = {
        {cand(1, 0, 0), cand(2, 1, 1), 0},  // BG1 P0 over BG2 P1
        {cand(1, 0, 1), cand(2, 1, 0), 0},  // BG1 P1 over BG2 P0
        {cand(1, 1, 1), cand(2, 2, 0), 0},  // BG2 P1 over BG3 P0
        {cand(1, 2, 0), cand(2, 1, 1), 1},  // BG2 P1 over BG3 P0 (swapped)
    };
    for (const Case& tc : cases) {
        const PixelCandidate pair[] = {tc.a, tc.b};
        EXPECT_EQ(compose(pair), tc.winner);
    }
}
