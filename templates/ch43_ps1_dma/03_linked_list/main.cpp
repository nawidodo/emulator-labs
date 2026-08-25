#define LABSTEST_MAIN
#include "labstest.hpp"
#include "linked_list.hpp"

using ps1::CaptureSink;
using ps1::Ram;
using ps1::WalkResult;

namespace {
// Chain: pkt0 (2 words) -> pkt1 (1 word) -> terminator.
void seed_two_packet_chain(Ram& ram) {
    // Packet layout: payloads follow their own header in memory.
    ram.write(0x0000, 0x02000014u);   // hdr: 2 words, next 0x14
    ram.write(0x0004, 0x11111111u);
    ram.write(0x0008, 0x22222222u);
    ram.write(0x0014, 0x01FFFFFFu);   // hdr: 1 word, TERMINATOR link
    ram.write(0x0018, 0x33333333u);
    // Stale garbage beyond the chain (never legitimately read).
    ram.write(0x0050, 0xDEADBEEFu);
    ram.write(0x0054, 0x00000001u);
}
}  // namespace

TEST(ll, otc_builds_backwards_table_with_exact_sentinel) {
    Ram ram;
    const uint32_t start = 0x400;
    const uint32_t count = 4;
    ps1::otc_build(ram, start, count);
    EXPECT_EQ(ram.read(start - 0), start - 4);
    EXPECT_EQ(ram.read(start - 4), start - 8);
    EXPECT_EQ(ram.read(start - 8), start - 12);
    EXPECT_EQ(ram.read(start - 12), 0x00FFFFFFu);  // exact sentinel
}

TEST(ll, header_decode_separates_count_and_link) {
    const uint32_t hdr = 0x03001234u;
    EXPECT_EQ(ps1::packet_word_count(hdr), 3u);
    EXPECT_EQ(ps1::packet_next_link(hdr), 0x001234u);
}

TEST(ll, walk_delivers_exact_payload_and_terminates) {
    Ram ram;
    seed_two_packet_chain(ram);
    CaptureSink gpu;
    ps1::TraceLog trace;
    const WalkResult r = ps1::walk_gpu_list(ram, 0x0000, gpu, &trace);
    EXPECT_TRUE(r.terminated);
    EXPECT_FALSE(r.cap_hit);
    EXPECT_EQ(r.packets, 2u);
    // EXACTLY the payload words: header never reaches the GPU.
    EXPECT_TRUE(gpu.words().size() == 3);
    EXPECT_EQ(gpu.words()[0], 0x11111111u);
    EXPECT_EQ(gpu.words()[1], 0x22222222u);
    EXPECT_EQ(gpu.words()[2], 0x33333333u);

    // Cycle model: (1+2) + hop(1) + (1+1) = 6.
    EXPECT_EQ(r.cycles, 6u);
    EXPECT_TRUE(trace.size() == 2);
    EXPECT_TRUE(trace[0].find("pkt=0 ptr=000000") != std::string::npos);
    EXPECT_TRUE(trace[1].find("ptr=000014 hdr=01FFFFFF words=1") !=
                std::string::npos);
}

TEST(ll, zero_length_packet_still_hops_and_counts_header) {
    Ram ram;
    ram.write(0x0000, 0x00000020u);            // EMPTY packet, next 0x20
    ram.write(0x0020, 0x01000030u);            // 1 payload word, next 0x30
    ram.write(0x0024, 0xABCD0000u);            // that payload
    ram.write(0x0030, ps1::kListTerminator);   // zero-length final header
    CaptureSink gpu;
    const WalkResult r = ps1::walk_gpu_list(ram, 0x0000, gpu);
    EXPECT_TRUE(r.terminated);
    EXPECT_FALSE(r.cap_hit);
    // The zero-length sentinel header still counts as a visited packet.
    EXPECT_EQ(r.packets, 3u);
    EXPECT_TRUE(gpu.words().size() == 1);
    EXPECT_EQ(gpu.words()[0], 0xABCD0000u);
    // cycles: hdr(1) + hop(1) + hdr+word(2) + hop(1) + hdr(1) = 6.
    EXPECT_EQ(r.cycles, 6u);
}

TEST(ll, cap_guard_fires_on_circular_chain) {
    Ram ram;
    ram.write(0x0000, 0x00000000u);  // self loop: next = 0x0000
    CaptureSink gpu;
    const WalkResult r = ps1::walk_gpu_list(ram, 0x0000, gpu, nullptr, 8);
    EXPECT_FALSE(r.terminated);
    EXPECT_TRUE(r.cap_hit);
    EXPECT_EQ(r.packets, 8u);
}
