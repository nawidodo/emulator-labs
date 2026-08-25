// Tests for exercise 03: STAT interrupt sources and edge detection.
#define LABSTEST_MAIN
#include <vector>

#include "labstest.hpp"
#include "stat.hpp"

using gbstat::EdgeDetector;

namespace {

// Walk one full frame line by line with the given enables and LYC,
// recording every line where the edge detector fires. Mode model matches
// exercise 02: mode 2 at each visible line start, vblank on 144..153.
std::vector<int> irqLines(int lyc, bool lycEn, bool oamEn, bool vbEn,
                          bool hbEn) {
    std::vector<int> fired;
    EdgeDetector d;
    for (int ly = 0; ly < 154; ++ly) {
        const int mode = (ly >= 144) ? 1 : 2;  // sample at dot 0
        const bool sig = gbstat::statSignal(lycEn, oamEn, vbEn, hbEn,
                                            gbstat::coincidenceFlag(ly, lyc),
                                            mode);
        if (gbstat::feed(d, sig)) fired.push_back(ly);
    }
    return fired;
}

}  // namespace

TEST(stat, signal_or_of_enabled_sources) {
    EXPECT_FALSE(gbstat::statSignal(false, false, false, false, true, 0));
    EXPECT_TRUE(gbstat::statSignal(true, false, false, false, true, 0));
    EXPECT_FALSE(gbstat::statSignal(true, false, false, false, false, 3));
    EXPECT_TRUE(gbstat::statSignal(false, true, false, false, false, 2));
    EXPECT_FALSE(gbstat::statSignal(false, true, false, false, false, 3));
    EXPECT_TRUE(gbstat::statSignal(false, false, true, false, false, 1));
    EXPECT_TRUE(gbstat::statSignal(false, false, false, true, false, 0));
    // Two sources at once still one line.
    EXPECT_TRUE(gbstat::statSignal(true, true, false, false, true, 2));
}

TEST(stat, coincidence_is_plain_equality) {
    EXPECT_TRUE(gbstat::coincidenceFlag(64, 64));
    EXPECT_TRUE(gbstat::coincidenceFlag(0, 0));
    EXPECT_TRUE(gbstat::coincidenceFlag(153, 153));
    EXPECT_FALSE(gbstat::coincidenceFlag(63, 64));
    EXPECT_FALSE(gbstat::coincidenceFlag(65, 64));
}

TEST(edge, fires_only_on_rising) {
    EdgeDetector d;
    EXPECT_FALSE(gbstat::feed(d, false));
    EXPECT_TRUE(gbstat::feed(d, true));   // 0 -> 1
    EXPECT_FALSE(gbstat::feed(d, true));  // held high
    EXPECT_FALSE(gbstat::feed(d, false));
    EXPECT_TRUE(gbstat::feed(d, true));   // second rise
}

TEST(edge, back_to_back_sources_collapse_to_one_irq) {
    EdgeDetector d;
    int fires = 0;
    // OAM scan (mode 2) hands over to drawing then hblank with the hblank
    // source enabled: the STAT line never drops between modes 2 and 0 of
    // the same frame when sampled coarsely — model two adjacent highs.
    fires += gbstat::feed(d, true) ? 1 : 0;
    fires += gbstat::feed(d, true) ? 1 : 0;
    EXPECT_EQ(fires, 1);
}

namespace {

// Index a fire log safely: on a stub path the log may be empty, and the
// tests must stay RED (not crash) there.
int at(const std::vector<int>& v, size_t i) {
    return i < v.size() ? v[i] : -1;
}

}  // namespace

TEST(script, lyc_irq_exactly_once_at_the_match_line) {
    const auto fired = irqLines(64, true, false, false, false);
    EXPECT_EQ(fired.size(), static_cast<size_t>(1));
    EXPECT_EQ(at(fired, 0), 64);
}

TEST(script, lyc_zero_fires_at_line_0_only) {
    const auto fired = irqLines(0, true, false, false, false);
    EXPECT_EQ(fired.size(), static_cast<size_t>(1));
    EXPECT_EQ(at(fired, 0), 0);
}

TEST(script, vblank_source_fires_on_line_144) {
    const auto fired = irqLines(255, false, false, true, false);
    EXPECT_EQ(fired.size(), static_cast<size_t>(1));
    EXPECT_EQ(at(fired, 0), 144);
}

TEST(script, oam_source_fires_once_per_frame_per_rise) {
    // The OAM source is high at the start of every visible line, so the
    // OR-ed line stays high across lines — the edge detector fires once
    // per frame (line 0), again only after vblank drops the line.
    std::vector<int> fired;
    EdgeDetector d;
    for (int ly = 0; ly < 154 * 2; ++ly) {
        const int line = ly % 154;
        const int mode = (line >= 144) ? 1 : 2;  // sample at dot 0
        const bool sig = gbstat::statSignal(false, true, false, false,
                                            false, mode);
        if (gbstat::feed(d, sig)) fired.push_back(ly);
    }
    EXPECT_EQ(fired.size(), static_cast<size_t>(2));
    EXPECT_EQ(at(fired, 0), 0);
    EXPECT_EQ(at(fired, 1), 154);
}

TEST(script, combined_lyc_and_vblank_two_interrupts) {
    // LYC = 100 and vblank enabled: rising edges at line 100 and at the
    // vblank boundary — but only if the signal dropped in between.
    const auto fired = irqLines(100, true, false, true, false);
    EXPECT_EQ(fired.size(), static_cast<size_t>(2));
    EXPECT_EQ(at(fired, 0), 100);
    EXPECT_EQ(at(fired, 1), 144);
}
