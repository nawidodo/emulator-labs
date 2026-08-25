#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_ll.hpp"

using ps1dbg::Ram;
using ps1dbg::Result;
using ps1dbg::Sink;

namespace {
void seed_chain(Ram& ram) {
    ram.write(0x000, 0x02000010u);  // hdr: 2 words -> next 0x010
    ram.write(0x004, 0x11111111u);
    ram.write(0x008, 0x22222222u);
    ram.write(0x010, 0x01FFFFFFu);  // hdr: 1 word -> terminator link
    ram.write(0x014, 0x33333333u);
    // Stale garbage past the chain end (never legitimately read).
}
}  // namespace

TEST(debug_ll, terminates_on_exact_sentinel) {
    Ram ram;
    seed_chain(ram);
    Sink sink;
    const Result r = ps1dbg::walk(ram, 0x000, sink);
    EXPECT_TRUE(r.terminated);
    EXPECT_EQ(r.packets, 2u);
}

TEST(debug_ll, gpu_receives_payload_only_in_order) {
    Ram ram;
    seed_chain(ram);
    Sink sink;
    ps1dbg::walk(ram, 0x000, sink);
    EXPECT_TRUE(sink.got.size() == 3);
    EXPECT_EQ(sink.got[0], 0x11111111u);
    EXPECT_EQ(sink.got[1], 0x22222222u);
    EXPECT_EQ(sink.got[2], 0x33333333u);
}

TEST(debug_ll, no_stale_reads_beyond_chain) {
    Ram ram;
    seed_chain(ram);
    Sink sink;
    ps1dbg::walk(ram, 0x000, sink);
    for (const uint32_t w : sink.got)
        EXPECT_NE(w, Ram::kDeadPattern);
}
