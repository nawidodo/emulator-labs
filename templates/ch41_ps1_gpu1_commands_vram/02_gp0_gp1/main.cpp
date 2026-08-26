#define LABSTEST_MAIN
#include "labstest.hpp"
#include <memory>

#include <cstdint>

#include "gpu.hpp"

using psx::gpu::Gpu;

namespace {
constexpr uint32_t kFillWhite = 0x02FFFFFF;  // GP0(02h) header + white
constexpr uint32_t kCoord = 0;               // xy = (0,0)
// Size parameter word: height in bits 16-31, width in bits 0-15.
constexpr uint32_t kWH(uint32_t w, uint32_t h) { return (h << 16) | w; }
}  // namespace

TEST(gpustat, poweron_reads_14802000) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.reset();
    EXPECT_EQ(g.status(), 0x14802000u);
}

TEST(gpustat, display_enable_clears_bit23) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    EXPECT_EQ((g.status() >> 23) & 1, 1u);  // off at power-on
    g.write_gp1(0x03000000u);               // GP1(03h): on
    EXPECT_EQ((g.status() >> 23) & 1, 0u);
    g.write_gp1(0x03000001u);               // off again
    EXPECT_EQ((g.status() >> 23) & 1, 1u);
}

TEST(gpustat, dma_dir_and_drq_follow_gp1_04) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp1(0x04000002u);               // direction 2 (FIFO empty)
    EXPECT_EQ((g.status() >> 29) & 3, 2u);
    EXPECT_EQ((g.status() >> 25) & 1, 1u);  // FIFO drained -> DRQ
    g.write_gp1(0x04000000u);               // off: DRQ forced low
    EXPECT_EQ((g.status() >> 29) & 3, 0u);
    EXPECT_EQ((g.status() >> 25) & 1, 0u);
}

TEST(gpustat, irq_set_by_gp0_1f_ackd_by_gp1_02) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0x1F000000u);
    EXPECT_EQ((g.status() >> 24) & 1, 1u);
    g.write_gp1(0x02000000u);
    EXPECT_EQ((g.status() >> 24) & 1, 0u);
}

TEST(gpustat, e6_mask_bits_mirror_to_11_12) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xE6000003u);
    EXPECT_EQ((g.status() >> 11) & 3, 3u);
}

TEST(gpustat, e1_draw_mode_maps_low_bits) {
    // draw mode 0x4A = 0100.1010b: texpage X=10 (bits 0-3), everything
    // else in bits 4-10 clear.
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xE100004Au);
    EXPECT_EQ(g.status() & 0xF, 0xAu);
    EXPECT_EQ((g.status() >> 9) & 1, 0u);   // dither still off
}

TEST(gp1, reset_restores_poweron_but_keeps_vram) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(kFillWhite);
    g.write_gp0(kCoord);
    g.write_gp0(kWH(0x3FF, 0x1FF));         // near-full-screen fill
    g.write_gp1(0x03000000u);               // display on
    g.write_gp1(0x00000000u);               // reset
    EXPECT_EQ(g.status(), 0x14802000u);     // display off again...
    EXPECT_NE(g.vram.at(5, 5), 0u);         // ...but VRAM survives
}

TEST(gp1, reset_command_buffer_aborts_partial_packet) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    // Start a FILL packet (header + color) but withhold its last parameter:
    // the parser must hold the incomplete packet until GP1(01h) aborts it.
    g.write_gp0(0x02808080u);               // header, cmd 02h
    g.write_gp0(0x00100010u);               // xy parameter
    EXPECT_EQ(g.pending_words(), size_t{0});  // FIFO drained; parser holds
                                              // the partial packet instead
    g.write_gp1(0x01000000u);               // abort
    g.write_gp0(0x60000000u);               // next header parses cleanly
    EXPECT_EQ((g.status() >> 26) & 1, 1u);  // ready for commands
}

TEST(fill, param_400h_collapses_to_no_fill) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(kFillWhite);
    g.write_gp0(kCoord);
    g.write_gp0(kWH(0x400, 0x200));         // literal max: BOTH wrap to 0
    EXPECT_EQ(g.vram.at(0, 0), 0u);         // documented quirk: no fill
    EXPECT_EQ(g.vram.at(1023, 511), 0u);
}

TEST(fill, param_3ff_rounds_up_to_full_width) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(kFillWhite);
    g.write_gp0(kCoord);
    g.write_gp0(kWH(0x3FF, 0x1FF));         // width rounds up to 400h
    // Height 0x1FF stays 0x1FF (=511 rows): row 511 untouched.
    EXPECT_EQ(g.vram.at(1023, 0), 0x7FFFu);
    EXPECT_EQ(g.vram.at(1023, 510), 0x7FFFu);
    EXPECT_EQ(g.vram.at(500, 511), 0u);
}

TEST(fill, xpos_aligns_down_to_16_and_region_wraps) {
    const uint16_t green = 0x03E0u;         // G=FF -> 5bit 11111 << 5
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0x0200FF00u);               // green fill
    g.write_gp0((300u << 16) | 8u);         // xy = (8, 300); x aligns to 0
    // x=8 aligns down to 0; y=300 with height 300 runs past the bottom edge
    // and wraps to the top of VRAM (no carry between axes).
    g.write_gp0(kWH(48, 300));
    EXPECT_EQ(g.vram.at(0, 300), green);
    EXPECT_EQ(g.vram.at(47, 300), green);
    EXPECT_EQ(g.vram.at(48, 300), 0u);
    EXPECT_EQ(g.vram.at(5, 87), green);     // wrapped tail row
    EXPECT_EQ(g.vram.at(5, 88), 0u);
    EXPECT_EQ(g.vram.at(5, 299), 0u);
}

TEST(xfer, cpu_to_vram_stream_wraps_columns) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xA0000000u);
    g.write_gp0((50u << 10) | 1020u);       // dest (1020, 50)
    g.write_gp0(kWH(8, 2));                 // 16 halfwords
    for (uint32_t i = 0; i < 8; ++i)
        g.write_gp0(((0x100u + 2 * i + 1) << 16) | (0x100u + 2 * i));
    EXPECT_EQ(g.vram.at(1020, 50), 0x100u);
    EXPECT_EQ(g.vram.at(1023, 50), 0x103u); // low halves first
    EXPECT_EQ(g.vram.at(0, 50), 0x104u);    // wrapped column, same row
    EXPECT_EQ(g.vram.at(1020, 51), 0x108u); // next row
    EXPECT_EQ((g.status() >> 26) & 1, 1u);  // ready again after stream ends
}

TEST(xfer, odd_halfword_count_takes_low_halves_only) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xA0000000u);
    g.write_gp0(0u);                        // dest (0,0)
    g.write_gp0(kWH(3, 1));                 // 3 halfwords = odd
    g.write_gp0(0x000A000Bu);               // halves B,A
    g.write_gp0(0xABCD000Cu);               // half C (high half ignored)
    EXPECT_EQ(g.vram.at(0, 0), 0xBu);
    EXPECT_EQ(g.vram.at(1, 0), 0xAu);
    EXPECT_EQ(g.vram.at(2, 0), 0xCu);
    EXPECT_EQ(g.vram.at(3, 0), 0u);
}

TEST(xfer, vram_to_cpu_gpuread_with_odd_padding) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xA0000000u);
    g.write_gp0(0u);
    g.write_gp0(kWH(3, 1));
    g.write_gp0(0x00020003u);               // pixels 3,2
    g.write_gp0(0xFFFF0001u);               // pixel 1 (odd tail)
    g.write_gp0(0xC0000000u);               // download (0,0) 3x1
    g.write_gp0(0u);                        // src (0,0)
    g.write_gp0(kWH(3, 1));                 // 3 halfwords
    const bool have_data = ((g.status() >> 27) & 1) == 1;
    EXPECT_TRUE(have_data);                 // data available
    // NOTE: hoist side-effecting calls — EXPECT_EQ evaluates its arguments
    // more than once.
    const uint32_t w0 = g.gpuread();
    const uint32_t w1 = g.gpuread();
    EXPECT_EQ(w0, 0x00020003u);             // hw0 low, hw1 high
    EXPECT_EQ(w1, 0x00000001u);             // lone halfword padded
    const bool drained = ((g.status() >> 27) & 1) == 0;
    EXPECT_TRUE(drained);                   // exhausted
    EXPECT_EQ(g.gpuread(), 0u);
}

TEST(xfer, vram_to_vram_packet_via_fifo) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    // Seed a 4x2 block at (10,20) via an upload, then copy it to (60,40)
    // through a full GP0(80h) packet: header + src + dst + wh.
    g.write_gp0(0xA0000000u);
    g.write_gp0((20u << 10) | 10u);
    g.write_gp0(kWH(4, 2));
    for (uint32_t i = 0; i < 4; ++i)
        g.write_gp0(((0x200u + 2 * i + 1) << 16) | (0x200u + 2 * i));
    g.write_gp0(0x80000000u);
    g.write_gp0((20u << 10) | 10u);         // src (10,20)
    g.write_gp0((40u << 10) | 60u);         // dst (60,40)
    g.write_gp0(kWH(4, 2));
    EXPECT_EQ(g.vram.at(60, 40), 0x200u);
    EXPECT_EQ(g.vram.at(63, 40), 0x203u);
    EXPECT_EQ(g.vram.at(60, 41), 0x204u);
}


TEST(xfer, new_download_replaces_pending_read_fifo) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    g.write_gp0(0xA0000000u);
    g.write_gp0(0u);
    g.write_gp0(kWH(2, 1));
    g.write_gp0(0x000000AAu);
    g.write_gp0(0xC0000000u);               // first download
    g.write_gp0(0u);
    g.write_gp0(kWH(2, 1));
    g.write_gp0(0xC0000000u);               // second download, never read
    g.write_gp0(0u);
    g.write_gp0(kWH(2, 1));
    EXPECT_EQ(g.gpuread(), 0x000000AAu);    // fresh latch, not a queue
}

TEST(packet, synchronous_model_drains_fifo_per_write) {
    auto g_storage = std::make_unique<Gpu>();
    Gpu& g = *g_storage;
    // Open a large upload (needs 102400 data words) and stream a few words:
    // in this chapter's zero-latency model every write drains immediately,
    // so the 16-word ring never backs up.
    g.write_gp0(0xA0000000u);
    g.write_gp0(0u);
    g.write_gp0(kWH(400, 512));
    bool all_accepted = true;
    for (int i = 0; i < 19; ++i) all_accepted &= g.write_gp0(0xFFFFFFFFu);
    EXPECT_TRUE(all_accepted);
    EXPECT_EQ(g.pending_words(), size_t{0});
}
