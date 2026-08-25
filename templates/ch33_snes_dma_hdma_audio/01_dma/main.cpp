// Unit tests for the 01_dma DMA channel model.
// Golden expectations are hand-derived from the canonical mode table in
// LECTURE.md (sources: Anomie's register doc + SNESdev Wiki "DMA").
#define LABSTEST_MAIN
#include "labstest.hpp"
#include "dma.hpp"
#include <cstddef>

#include <array>

using snesdma::AStep;
using snesdma::Channel;
using snesdma::TransferStep;

namespace {

std::array<uint8_t, 16> make_source() {
    std::array<uint8_t, 16> bus{};
    for (size_t i = 0; i < bus.size(); ++i) bus[i] = uint8_t(0x10 + i);
    return bus;
}

Channel base_channel(uint8_t control) {
    Channel ch;
    ch.control = control;
    ch.b_reg = 0x18;  // -> $2118, the classic VRAM pair low register
    ch.a_addr = 0x0000;
    ch.a_bank = 0x00;
    ch.unit_count = 8;
    return ch;
}

}  // namespace

TEST(DmaModes, UnitsTable) {
    const int expected[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    for (uint8_t mode = 0; mode < 8; ++mode) {
        EXPECT_EQ(snesdma::units_per_transfer(mode), expected[mode]);
    }
}

TEST(DmaModes, BOffsetTable) {
    using Row = std::array<uint8_t, 4>;
    const Row table[8] = {
        {0, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
    };
    const int units[8] = {1, 2, 2, 4, 4, 4, 2, 4};
    for (uint8_t mode = 0; mode < 8; ++mode) {
        for (int u = 0; u < units[mode]; ++u) {
            EXPECT_EQ(snesdma::unit_b_offset(mode, u), table[mode][u]);
        }
    }
}

TEST(DmaModes, AStepDecode) {
    EXPECT_TRUE(snesdma::a_step_kind(0x00) == AStep::Increment);   // mode 0
    EXPECT_TRUE(snesdma::a_step_kind(0x08) == AStep::Fixed);       // bits=01
    EXPECT_TRUE(snesdma::a_step_kind(0x10) == AStep::Decrement);   // bits=10
    EXPECT_TRUE(snesdma::a_step_kind(0x18) == AStep::Fixed);       // bits=11
    // Modes 6/7 force decrement no matter what bits 4-3 say.
    EXPECT_TRUE(snesdma::a_step_kind(0x06) == AStep::Decrement);
    EXPECT_TRUE(snesdma::a_step_kind(0x07) == AStep::Decrement);
    EXPECT_TRUE(snesdma::a_step_kind(0x07 | 0x08) == AStep::Decrement);
}

TEST(DmaModes, ModeSequences) {
    const auto bus = make_source();
    // Base $2118: full expected B-address sequence for an 8-byte transfer.
    // Modes 6/7 are covered separately (they force A decrement, so the
    // walk starts near the top of the bus image instead of at 0).
    struct Case {
        uint8_t mode;
        std::array<uint16_t, 8> b_addrs;
    };
    const Case cases[6] = {
        {0, {0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118}},
        {1, {0x2118, 0x2119, 0x2118, 0x2119, 0x2118, 0x2119, 0x2118, 0x2119}},
        {2, {0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118, 0x2118}},
        {3, {0x2118, 0x2118, 0x2119, 0x2119, 0x2118, 0x2118, 0x2119, 0x2119}},
        {4, {0x2118, 0x2119, 0x211A, 0x211B, 0x2118, 0x2119, 0x211A, 0x211B}},
        {5, {0x2118, 0x2119, 0x2118, 0x2119, 0x2118, 0x2119, 0x2118, 0x2119}},
    };
    for (const auto& c : cases) {
        const auto log = snesdma::run_channel(base_channel(c.mode), bus);
        EXPECT_EQ(log.size(), size_t(8));
        if (log.size() != 8) continue;
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(log[size_t(i)].b_addr, c.b_addrs[size_t(i)]);
            // Incrementing A by default: byte i lives at a_addr = i.
            EXPECT_EQ(log[size_t(i)].a_addr, uint32_t(i));
        }
    }
}

TEST(DmaModes, ForcedDecrementModeSequences) {
    const auto bus = make_source();
    // Modes 6/7 force A decrement: start at 0x000F so the walk stays in
    // the 16-byte image for all 8 bytes (0xF down to 0x8).
    Channel ch = base_channel(0x06);
    ch.a_addr = 0x000F;
    const auto log6 = snesdma::run_channel(ch, bus);
    EXPECT_EQ(log6.size(), size_t(8));
    if (log6.size() == 8) {
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(log6[size_t(i)].b_addr, uint16_t(0x2118));  // +0,+0
            EXPECT_EQ(log6[size_t(i)].a_addr, uint32_t(0xF - i));
        }
    }
    ch.control = 0x07;  // mode 7: pattern +0,+0,+1,+1
    const auto log7 = snesdma::run_channel(ch, bus);
    EXPECT_EQ(log7.size(), size_t(8));
    if (log7.size() == 8) {
        const uint16_t want[8] = {0x2118, 0x2118, 0x2119, 0x2119,
                                  0x2118, 0x2118, 0x2119, 0x2119};
        for (int i = 0; i < 8; ++i) {
            EXPECT_EQ(log7[size_t(i)].b_addr, want[i]);
            EXPECT_EQ(log7[size_t(i)].a_addr, uint32_t(0xF - i));
        }
    }
}

TEST(DmaModes, FixedAddressFill) {
    const auto bus = make_source();
    Channel ch = base_channel(0x08);  // mode 0, bits 4-3 = 01 => fixed
    const auto log = snesdma::run_channel(ch, bus);
    EXPECT_EQ(log.size(), size_t(8));
    for (const auto& s : log) {
        EXPECT_EQ(s.b_addr, uint16_t(0x2118));
        EXPECT_EQ(s.a_addr, uint32_t(0));  // never moves
    }
}

TEST(DmaModes, DecrementWalk) {
    const auto bus = make_source();
    Channel ch = base_channel(0x10);  // mode 0, bits 4-3 = 10 => decrement
    ch.a_addr = 0x0007;
    const auto log = snesdma::run_channel(ch, bus);
    EXPECT_EQ(log.size(), size_t(8));
    if (log.size() != 8) return;
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(log[size_t(i)].a_addr, uint32_t(7 - i));
    }
}

TEST(DmaModes, PartialLogAtImageEnd) {
    const auto bus = make_source();  // only $10 bytes mapped
    Channel ch = base_channel(0x01);  // mode 1, 2 bytes per unit
    ch.a_bank = 0x00;
    ch.a_addr = 0x000E;  // bytes at flat $000E/$000F exist, $0010 does not
    ch.unit_count = 8;
    const auto log = snesdma::run_channel(ch, bus);
    // Two steps survive, then the channel halts off the end of the image.
    EXPECT_EQ(log.size(), size_t(2));
    if (log.size() != 2) return;
    EXPECT_EQ(log[0].b_addr, uint16_t(0x2118));
    EXPECT_EQ(log[0].a_addr, uint32_t(0x000E));
    EXPECT_EQ(log[1].b_addr, uint16_t(0x2119));
    EXPECT_EQ(log[1].a_addr, uint32_t(0x000F));
}
