#define LABSTEST_MAIN
#include "labstest.hpp"
#include "challenge.hpp"
#include <cstddef>

using ps1::Ram;
using ps1chal::Vram;

namespace {
// Chain of one FillRect packet: color 0x7C1F, rect (4,2) 8x5.
void seed_rect_chain(Ram& ram) {
    ram.write(0x00, 0x03000040u);         // 3 payload words -> next 0x40
    ram.write(0x04, 0x02007C1Fu);         // cmd word: FillRect, color low16
    ram.write(0x08, 0x00040002u);         // x=4 y=2
    ram.write(0x0C, 0x00080005u);         // w=8 h=5
    ram.write(0x40, ps1::kListTerminator);
}
}  // namespace

TEST(challenge, fill_rect_lands_in_vram) {
    Ram ram;
    seed_rect_chain(ram);
    Vram vram;
    const auto res = ps1chal::run_list(ram, 0, vram);
    EXPECT_TRUE(res.walk.terminated);
    EXPECT_EQ(res.walk.words, 3u);
    EXPECT_EQ(vram.px[size_t(2) * Vram::kW + 4], 0x7C1F);
    EXPECT_EQ(vram.px[size_t(6) * Vram::kW + 11], 0x7C1F);
    EXPECT_EQ(vram.px[size_t(7) * Vram::kW + 4], 0u);   // outside rect
}

TEST(challenge, vram_hash_matches_reference_run) {
    Ram ram;
    seed_rect_chain(ram);
    Vram a, b;
    const auto ra = ps1chal::run_list(ram, 0, a);
    const auto rb = ps1chal::run_list(ram, 0, b);
    EXPECT_EQ(ra.vram_fnv, rb.vram_fnv);  // deterministic: twice identical

    // Golden produced by the reference solution; provenance in
    // tests/public/ch43_ps1_dma/vram/provenance.md.
    EXPECT_EQ(ra.vram_fnv, 0x95D3D4F5F97E0225ull);  // GOLDEN_HASH
}

TEST(challenge, two_lists_compose_draw_order) {
    Ram ram;
    // Packet A at 0x00 (rect color 0x7C1F), chained into packet B at 0x40
    // (rect color 0x001F). Later draw wins inside the overlap.
    ram.write(0x00, 0x03000040u);
    ram.write(0x04, 0x02007C1Fu);
    ram.write(0x08, 0x00040002u);          // x=4 y=2
    ram.write(0x0C, 0x000A000Au);          // w=10 h=10 -> covers (7,7)
    ram.write(0x40, 0x03000060u);          // next packet at 0x60
    ram.write(0x44, 0x0200001Fu);          // FillRect blue
    ram.write(0x48, 0x00060006u);          // x=6 y=6
    ram.write(0x4C, 0x00040004u);          // w=4 h=4
    ram.write(0x60, ps1::kListTerminator);

    Vram vram;
    const auto res = ps1chal::run_list(ram, 0, vram);
    EXPECT_TRUE(res.walk.terminated);
    // The zero-length sentinel header counts as a visited packet.
    EXPECT_EQ(res.walk.packets, 3u);
    EXPECT_EQ(vram.px[size_t(7) * Vram::kW + 7], 0x001F);  // overlap: B wins
    EXPECT_EQ(vram.px[size_t(3) * Vram::kW + 5], 0x7C1F);  // only in A
}
