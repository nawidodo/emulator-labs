#define LABSTEST_MAIN
#include "labstest.hpp"

#include "machine.hpp"

#include <cstdint>
#include <vector>

// Challenge: boot the course-original SI-compatible diagnostic and verify
// its full acceptance contract — port traffic, shift-register math,
// interrupt cadence, VRAM painting by the RST 10 handler, watchdog kick
// and a deterministic final frame.
//
// The identical image ships as
// tests/public/ch09_space_invaders_machine/roms/ch09_si_diag.bin (listing
// in ch09_si_diag.asm.txt).

using namespace si;

namespace {

const uint8_t kDiagProgram[] = {
        0xC3, 0x40, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x7E, 0x00, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x8B, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xF3, 0x31, 0x00, 0x21, 0x3E, 0x03, 0xD3, 0x02,
        0x3E, 0xDE, 0xD3, 0x04, 0x3E, 0xAD, 0xD3, 0x04, 0xDB, 0x03, 0x32, 0x02,
        0x20, 0x3A, 0x02, 0x20, 0xFE, 0xBB, 0x3E, 0x01, 0xCA, 0x60, 0x00, 0x3D,
        0x32, 0x04, 0x20, 0xDB, 0x01, 0x32, 0x03, 0x20, 0x3E, 0x55, 0xD3, 0x03,
        0x3E, 0xAA, 0xD3, 0x05, 0xD3, 0x06, 0xFB, 0x01, 0x2C, 0x1A, 0x0B, 0x78,
        0xB1, 0xC2, 0x76, 0x00, 0xF3, 0x76, 0xF5, 0xE5, 0x2A, 0x10, 0x20, 0x23,
        0x22, 0x10, 0x20, 0xE1, 0xF1, 0xFB, 0xC9, 0xF5, 0xC5, 0xD5, 0xE5, 0x2A,
        0x12, 0x20, 0x23, 0x22, 0x12, 0x20, 0x7D, 0x5F, 0x16, 0x24, 0xEB, 0x7B,
        0x77, 0xE1, 0xD1, 0xC1, 0xF1, 0xFB, 0xC9,
};

struct DiagRig {
    SpaceInvadersMachine m;

    DiagRig() {
        m.load_rom(kDiagProgram, sizeof kDiagProgram);
        m.set_inputs({0x00, 0x04, 0x00});   // fire pressed from frame 0
        m.run(6 * kCyclesPerFrame, nullptr);
    }
};

}  // namespace

TEST(challenge, program_boots_and_halts_within_six_frames) {
    DiagRig r;
    EXPECT_TRUE(r.m.cpu().halted);
    EXPECT_TRUE(r.m.cpu().cycles < 6 * kCyclesPerFrame);
    EXPECT_TRUE(r.m.cpu().cycles > 160000);   // delay loop really ran ~5 frames
}

TEST(challenge, shift_register_round_trip_is_exact) {
    DiagRig r;
    // OUT 4 0xDE, OUT 4 0xAD with amount 3 -> IN 3 must read 0xBB.
    EXPECT_EQ(r.m.read(0x2002), 0xBB);
    // The program's own verdict byte: 01 = observed value matched.
    EXPECT_EQ(r.m.read(0x2004), 0x01);
}

TEST(challenge, input_latch_reached_the_program) {
    DiagRig r;
    EXPECT_EQ(r.m.read(0x2003), 0x04);     // IN 1 sampled the fire button
}

TEST(challenge, interrupt_counters_alternate_3_2) {
    DiagRig r;
    const int cnt8 = r.m.read(0x2010) | (r.m.read(0x2011) << 8);
    const int cnt10 = r.m.read(0x2012) | (r.m.read(0x2013) << 8);
    EXPECT_EQ(cnt8, 3);                    // boundaries at 32k/96k/160k
    EXPECT_EQ(cnt10, 2);                   // boundaries at 64k/128k
}

TEST(challenge, rst10_handler_painted_vram) {
    DiagRig r;
    // Handler writes counter LSB to VRAM[0x2400 + count].
    EXPECT_EQ(r.m.read(0x2400), 0x00);     // count was 1 -> wrote at +1
    EXPECT_EQ(r.m.vram().read(0x01), 0x01);
}

TEST(challenge, sound_events_and_watchdog_kick_recorded) {
    DiagRig r;
    const auto& ev = r.m.sound().events();
    EXPECT_TRUE(ev.size() >= 3u);
    bool saw3 = false, saw5 = false, saw6 = false;
    for (const auto& e : ev) {
        saw3 |= e.port == 3 && e.value == 0x55;
        saw5 |= e.port == 5 && e.value == 0xAA;
        saw6 |= e.port == 6;
    }
    EXPECT_TRUE(saw3);
    EXPECT_TRUE(saw5);
    EXPECT_TRUE(saw6);
    EXPECT_EQ(r.m.watchdog().last_kick(), ev.back().cycle);
}

TEST(challenge, irq_painted_pixel_reaches_the_frame) {
    DiagRig r;
    // The RST 10 handler wrote 0x01 into VRAM byte 1. Column-major:
    // bytes 0..31 are column 0, so byte 1 bit 0 lights upright pixel
    // (x=0, y=8). Ties acceptance to the real renderer, not just RAM.
    const size_t off = (size_t(8) * kScreenWidth + 0) * 4;
    EXPECT_EQ(r.m.frame().rgba[off + 0], 0xFF);
    EXPECT_EQ(r.m.frame().rgba[off + 3], 0xFF);
    EXPECT_EQ(r.m.frame().rgba[off - kScreenWidth * 4], 0x00);  // row above
}

TEST(challenge, frame_hash_is_deterministic) {
    DiagRig r;
    const uint64_t h = r.m.frame_hash();
    for (int i = 0; i < 3; ++i) {
        r.m.render();
        EXPECT_EQ(r.m.frame_hash(), h);
    }
    EXPECT_NE(h, 0u);                      // VRAM is painted, not blank
}
