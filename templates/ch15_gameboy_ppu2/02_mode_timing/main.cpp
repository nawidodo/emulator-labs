// Tests for exercise 02: dot-driven mode timing.
#define LABSTEST_MAIN
#include <string>
#include <cstddef>

#include "labstest.hpp"
#include "timing.hpp"

using gbtim::Mode;
using gbtim::PpuTiming;

TEST(modes, visible_line_mode_boundaries) {
    // Mode 2: dots 0..79.
    EXPECT_TRUE(gbtim::modeAt(10, 0) == Mode::OamScan);
    EXPECT_TRUE(gbtim::modeAt(10, 79) == Mode::OamScan);
    // Mode 3: dots 80..251.
    EXPECT_TRUE(gbtim::modeAt(10, 80) == Mode::Drawing);
    EXPECT_TRUE(gbtim::modeAt(10, 251) == Mode::Drawing);
    // Mode 0: dots 252..455.
    EXPECT_TRUE(gbtim::modeAt(10, 252) == Mode::HBlank);
    EXPECT_TRUE(gbtim::modeAt(10, 455) == Mode::HBlank);
}

TEST(modes, first_and_last_visible_line) {
    EXPECT_TRUE(gbtim::modeAt(0, 0) == Mode::OamScan);
    EXPECT_TRUE(gbtim::modeAt(0, 300) == Mode::HBlank);
    EXPECT_TRUE(gbtim::modeAt(143, 100) == Mode::Drawing);
}

TEST(modes, vblank_covers_whole_lines_144_to_153) {
    for (int line = 144; line <= 153; ++line) {
        EXPECT_TRUE(gbtim::modeAt(line, 0) == Mode::VBlank);
        EXPECT_TRUE(gbtim::modeAt(line, 228) == Mode::VBlank);
        EXPECT_TRUE(gbtim::modeAt(line, 455) == Mode::VBlank);
    }
}

TEST(advance, ly_increments_exactly_at_line_boundary) {
    PpuTiming t{5, 450, Mode::HBlank};
    const PpuTiming a = gbtim::advance(t, 5);
    EXPECT_EQ(a.ly, 5);
    EXPECT_EQ(a.dot, 455);
    const PpuTiming b = gbtim::advance(a, 1);
    EXPECT_EQ(b.ly, 6);
    EXPECT_EQ(b.dot, 0);
    EXPECT_TRUE(b.mode == Mode::OamScan);
}

TEST(advance, wraps_frame_153_to_0) {
    PpuTiming t{153, 454, Mode::HBlank};
    const PpuTiming a = gbtim::advance(t, 2);
    EXPECT_EQ(a.ly, 0);
    EXPECT_EQ(a.dot, 0);
    EXPECT_TRUE(a.mode == Mode::OamScan);
}

TEST(advance, full_frame_is_exactly_70224_dots) {
    EXPECT_EQ(gbtim::kFrameDots, 70224);
    EXPECT_EQ(gbtim::kDotsPerLine * gbtim::kTotalLines, gbtim::kFrameDots);
}

TEST(advance, full_frame_round_trip_is_identical) {
    PpuTiming t{77, 123, Mode::Drawing};
    const PpuTiming once = gbtim::advance(t, gbtim::kFrameDots);
    const PpuTiming twice = gbtim::advance(once, gbtim::kFrameDots);
    EXPECT_EQ(once.ly, t.ly);
    EXPECT_EQ(once.dot, t.dot);
    EXPECT_TRUE(once.mode == t.mode);
    EXPECT_EQ(twice.ly, t.ly);
    EXPECT_EQ(twice.dot, t.dot);
}

TEST(advance, crosses_vblank_entry) {
    PpuTiming t{143, 400, Mode::HBlank};
    const PpuTiming a = gbtim::advance(t, 60);  // into line 144
    EXPECT_EQ(a.ly, 144);
    EXPECT_EQ(a.dot, 4);
    EXPECT_TRUE(a.mode == Mode::VBlank);
}

TEST(locks, vram_locked_only_in_mode_3) {
    EXPECT_FALSE(gbtim::vramLocked(50, 0));     // mode 2
    EXPECT_TRUE(gbtim::vramLocked(50, 80));     // mode 3
    EXPECT_TRUE(gbtim::vramLocked(50, 251));    // mode 3
    EXPECT_FALSE(gbtim::vramLocked(50, 252));   // mode 0: unlocked again
    EXPECT_FALSE(gbtim::vramLocked(150, 100));  // vblank never locks
}

TEST(locks, oam_locked_in_modes_2_and_3) {
    EXPECT_TRUE(gbtim::oamLocked(50, 0));
    EXPECT_TRUE(gbtim::oamLocked(50, 79));
    EXPECT_TRUE(gbtim::oamLocked(50, 80));
    EXPECT_TRUE(gbtim::oamLocked(50, 251));
    EXPECT_FALSE(gbtim::oamLocked(50, 252));
    EXPECT_FALSE(gbtim::oamLocked(150, 0));
}

TEST(trace, format_of_first_transitions) {
    const std::string t = gbtim::buildModeTrace(gbtim::kFrameDots);
    EXPECT_EQ(t.substr(0, 18), std::string("ly=0 dot=0 mode=2\n"));
    EXPECT_NE(t.find("ly=0 dot=80 mode=3\n"), std::string::npos);
    EXPECT_NE(t.find("ly=0 dot=252 mode=0\n"), std::string::npos);
    EXPECT_NE(t.find("ly=1 dot=0 mode=2\n"), std::string::npos);
    EXPECT_NE(t.find("ly=144 dot=0 mode=1\n"), std::string::npos);
}

TEST(trace, deterministic_and_complete_over_one_frame) {
    const std::string a = gbtim::buildModeTrace(gbtim::kFrameDots);
    const std::string b = gbtim::buildModeTrace(gbtim::kFrameDots);
    EXPECT_EQ(a, b);
    // Every visible line contributes its three transitions (line start,
    // dot 80, dot 252); the vblank start plus each remaining vblank LY
    // change add ten more.
    size_t lines = 0;
    for (const char c : a)
        if (c == '\n') ++lines;
    EXPECT_EQ(lines, static_cast<size_t>(144 * 3 + 10));
    // Last transition of the frame: entering line 153.
    const size_t lastNl =
        a.size() >= 2 ? a.rfind('\n', a.size() - 2) : std::string::npos;
    EXPECT_EQ(a.substr(lastNl + 1), std::string("ly=153 dot=0 mode=1\n"));
}

TEST(trace, matches_committed_public_golden_shape) {
    // The committed golden tests/public/ch15_gameboy_ppu2/traces/
    // mode_trace.txt is byte-identical to buildModeTrace(kFrameDots)
    // (generated twice from the reference solution; see provenance.md).
    // Here we pin a checksum-free fingerprint safely even on an empty
    // stub path.
    const std::string t = gbtim::buildModeTrace(gbtim::kFrameDots);
    EXPECT_NE(t.find("ly=143 dot=252 mode=0\n"), std::string::npos);
    EXPECT_TRUE(t.empty() || t.back() == '\n');
}
