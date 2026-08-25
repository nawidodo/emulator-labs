// Tests for the 90_debug gradient engine. The expected write log is the
// committed golden (golden/gradient_writes.log, same values embedded here;
// see golden/provenance.md): line n carries brightness ramp(n).
#define LABSTEST_MAIN
#include "labstest.hpp"

#include "hdma_line.hpp"

#include <array>

using snesdma::debug::GradientHdma;
using snesdma::debug::LineWrite;

namespace {

// Brightness ramp: 0..15 INIDISP brightness stretched over 224 lines,
// held at 15 for the tail. Identical to fixtures/gradient_table.bin.
int ramp(int line) {
    const int v = line * 16 / 224;
    return v > 15 ? 15 : v;
}

GradientHdma make_engine() {
    std::array<uint8_t, size_t(snesdma::debug::kLines)> table{};
    for (int n = 0; n < snesdma::debug::kLines; ++n) {
        table[size_t(n)] = uint8_t(ramp(n));
    }
    GradientHdma hdma;
    hdma.load_table(table);
    return hdma;
}

}  // namespace

TEST(DebugGradient, FullLogMatchesGolden) {
    auto hdma = make_engine();
    const auto log = hdma.run();
    { EXPECT_TRUE(log.size() == size_t(snesdma::debug::kLines)); return; }
    for (int n = 0; n < snesdma::debug::kLines; ++n) {
        EXPECT_EQ(log[size_t(n)].line, n);
        EXPECT_EQ(log[size_t(n)].value, uint8_t(ramp(n)));
    }
}

TEST(DebugGradient, LineZeroGetsItsValue) {
    auto hdma = make_engine();
    const auto log = hdma.run();
    // First observable divergence of the bug: line 0 has NO write at all.
    EXPECT_EQ(log[0].line, 0);
    EXPECT_EQ(log[0].value, uint8_t(0));
}

TEST(DebugGradient, LineOneCarriesItsOwnData) {
    auto hdma = make_engine();
    const auto log = hdma.run();
    // With the bug, line 1 renders with table[0]'s value instead of its
    // own -- the gradient appears shifted down one scanline.
    EXPECT_EQ(log[1].value, uint8_t(ramp(1)));
}

TEST(DebugGradient, LastLineSeesFinalBrightness) {
    auto hdma = make_engine();
    const auto log = hdma.run();
    EXPECT_EQ(log.back().line, snesdma::debug::kLines - 1);
    EXPECT_EQ(log.back().value, uint8_t(15));
}
