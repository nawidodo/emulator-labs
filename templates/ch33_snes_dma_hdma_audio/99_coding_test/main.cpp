// Public tests for the 99_coding_test unseen transfer pattern. Every
// expectation below is derived from the worked example in SPEC.md; the
// hidden grader runs this same binary filtered to the VariantSpec suite.
#define LABSTEST_MAIN
#include "labstest.hpp"

#include "coding.hpp"

#include <array>

using snesdma::variant::Channel;
using snesdma::variant::TransferStep;

namespace {

// The SPEC worked example starts at flat A address $1000, so the bus
// image must span at least $1010 bytes. Function-static and zero-filled:
// deterministic, no per-test copies.
std::span<const uint8_t> make_source() {
    static const std::array<uint8_t, 0x1100> bus{};
    return bus;
}

Channel spec_channel() {
    // SPEC.md worked example: base $2102, A = $00:1000, 9 bytes.
    Channel ch;
    ch.control = 0x00;
    ch.b_reg = 0x02;
    ch.a_addr = 0x1000;
    ch.a_bank = 0x00;
    ch.unit_count = 9;
    return ch;
}

}  // namespace

TEST(VariantSpec, UnitOffsets) {
    EXPECT_EQ(snesdma::variant::kUnitsPerTransferX, 4);
    EXPECT_EQ(snesdma::variant::unit_b_offset_x(0), uint8_t(0));
    EXPECT_EQ(snesdma::variant::unit_b_offset_x(1), uint8_t(1));
    EXPECT_EQ(snesdma::variant::unit_b_offset_x(2), uint8_t(2));
    EXPECT_EQ(snesdma::variant::unit_b_offset_x(3), uint8_t(1));
}

TEST(VariantSpec, WorkedExampleSequence) {
    const auto bus = make_source();
    const auto log = snesdma::variant::run_mode_x(spec_channel(), bus);
    EXPECT_EQ(log.size(), size_t(9));
    if (log.size() != 9) return;
    // Exact SPEC.md table:
    struct Row {
        uint16_t b;
        uint16_t a;
    };
    const Row expected[9] = {
        {0x2102, 0x1000}, {0x2103, 0x1001}, {0x2104, 0x1002},
        {0x2103, 0x1003}, {0x2102, 0x1004}, {0x2103, 0x1005},
        {0x2104, 0x1006}, {0x2103, 0x1007}, {0x2102, 0x1008},
    };
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(log[size_t(i)].b_addr, expected[i].b);
        EXPECT_EQ(log[size_t(i)].a_addr, uint32_t(expected[i].a));
    }
}

TEST(VariantSpec, ForcedIncrementIgnoresControlBits) {
    const auto bus = make_source();
    Channel ch = spec_channel();
    ch.control = 0x08 | 0x10;  // claim fixed/decrement -- mode X ignores it
    const auto log = snesdma::variant::run_mode_x(ch, bus);
    EXPECT_EQ(log.size(), size_t(9));
    if (log.size() != 9) return;
    for (int i = 0; i < 9; ++i) {
        EXPECT_EQ(log[size_t(i)].a_addr, uint32_t(0x1000 + i));
    }
}

TEST(VariantSpec, PatternRepeatsAcrossUnits) {
    const auto bus = make_source();
    Channel ch = spec_channel();
    ch.unit_count = 12;  // exactly three full patterns
    const auto log = snesdma::variant::run_mode_x(ch, bus);
    EXPECT_EQ(log.size(), size_t(12));
    if (log.size() != 12) return;
    const int want[12] = {0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1};
    for (int i = 0; i < 12; ++i) {
        EXPECT_EQ(log[size_t(i)].b_addr,
                  uint16_t(0x2102 + want[i]));
    }
}

TEST(VariantSpec, PartialLogAtImageEnd) {
    // The image spans flat offsets $000..$10FF. A transfer starting at
    // offset $10FC survives four bytes and then halts (partial sequence).
    const auto bus = make_source();
    Channel ch = spec_channel();
    ch.a_bank = 0x00;
    ch.a_addr = 0x10FC;
    ch.unit_count = 8;
    const auto log = snesdma::variant::run_mode_x(ch, bus);
    EXPECT_EQ(log.size(), size_t(4));
    if (log.size() != 4) return;
    EXPECT_EQ(log[0].b_addr, uint16_t(0x2102));
    EXPECT_EQ(log[0].a_addr, uint32_t(0x10FC));
    EXPECT_EQ(log[3].a_addr, uint32_t(0x10FF));
}
