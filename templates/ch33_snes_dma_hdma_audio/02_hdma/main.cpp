// Unit tests for the 02_hdma scanline effect engine.
#include "hdma.hpp"

#define LABSTEST_MAIN
#include "labstest.hpp"
#include <array>

using snesdma::ChannelConfig;
using snesdma::Hdma;
using snesdma::RegWrite;

namespace {

ChannelConfig direct_channel(uint16_t base_reg) {
    ChannelConfig cfg;
    cfg.enabled = true;
    cfg.indirect = false;
    cfg.base_reg = base_reg;
    cfg.regs_per_line = 1;
    cfg.table_addr = 0;
    return cfg;
}

Hdma make_engine(std::span<const uint8_t> ram,
                 std::initializer_list<ChannelConfig> cfgs) {
    Hdma hdma;
    hdma.set_ram(ram);
    int ch = 0;
    for (const auto& c : cfgs) hdma.configure(ch++, c);
    return hdma;
}

}  // namespace

TEST(HdmaHeader, Decode) {
    auto t = snesdma::parse_header(0x00);
    EXPECT_TRUE(t.terminate);
    auto fresh = snesdma::parse_header(0x05);
    EXPECT_EQ(fresh.lines, 5);
    EXPECT_FALSE(fresh.repeat);
    EXPECT_FALSE(fresh.terminate);
    auto rep = snesdma::parse_header(0x85);
    EXPECT_EQ(rep.lines, 5);
    EXPECT_TRUE(rep.repeat);
    // $80: repeat with count 0 must not stall -> treated as terminate.
    auto bad = snesdma::parse_header(0x80);
    EXPECT_TRUE(bad.terminate);
}

TEST(HdmaDirect, FreshDataEveryLineAndTermination) {
    // Two lines with values 1,2 then termination.
    const std::array<uint8_t, 5> ram{0x02, 0x01, 0x02, 0x00, 0xFF};
    auto hdma = make_engine(ram, {direct_channel(0x00)});  // INIDISP $2100
    hdma.init();
    auto l0 = hdma.run_line(0);
    { EXPECT_TRUE(l0.size() == 1); return; }
    EXPECT_EQ(l0[0].reg, uint16_t(0x2100));
    EXPECT_EQ(l0[0].value, uint8_t(1));
    auto l1 = hdma.run_line(1);
    { EXPECT_TRUE(l1.size() == 1); return; }
    EXPECT_EQ(l1[0].value, uint8_t(2));
    // Header $00 consumed on line 2: terminated, nothing ever again.
    EXPECT_TRUE(hdma.run_line(2).empty());
    EXPECT_TRUE(hdma.run_line(3).empty());
    EXPECT_TRUE(hdma.run_line(223).empty());
}

TEST(HdmaRepeat, HoldsValueWithoutRewrites) {
    // Entry: repeat 3 lines, value 7. Only line 0 gets a write; lines 1-2
    // emit NOTHING (register keeps its value, like hardware).
    const std::array<uint8_t, 2> ram{uint8_t(0x83), 0x07};
    auto hdma = make_engine(ram, {direct_channel(0x02)});
    hdma.init();
    auto l0 = hdma.run_line(0);
    { EXPECT_TRUE(l0.size() == 1); return; }
    EXPECT_EQ(l0[0].value, uint8_t(7));   // fetched once, on the first line
    EXPECT_TRUE(hdma.run_line(1).empty());
    EXPECT_TRUE(hdma.run_line(2).empty());
    // Entry exhausted: channel terminates (end of table).
    EXPECT_TRUE(hdma.run_line(3).empty());
}

TEST(HdmaIndirect, PointerThenData) {
    // Table at 0: header $02, pointers to 0x10 then 0x12. Data area holds
    // AA BB CC DD. Flat image doubles as the indirect address space.
    const std::array<uint8_t, 20> ram{
        0x02, 0x10, 0x00,       // ptr 0x0010
        0x12, 0x00,             // ptr 0x0012
        0x00,                   // terminate
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0xAA, 0xBB,             // [0x10..0x11] line 0 data (regs=2)
        0xCC, 0xDD,             // [0x12..0x13] line 1 data
    };
    ChannelConfig cfg = direct_channel(0x21);  // $2121 CGRAM addr reg pair
    cfg.indirect = true;
    cfg.regs_per_line = 2;
    cfg.bank = 0x00;
    auto hdma = make_engine(ram, {cfg});
    hdma.init();
    auto l0 = hdma.run_line(0);
    { EXPECT_TRUE(l0.size() == 2); return; }
    EXPECT_EQ(l0[0].reg, uint16_t(0x2121));
    EXPECT_EQ(l0[0].value, uint8_t(0xAA));
    EXPECT_EQ(l0[1].reg, uint16_t(0x2122));
    EXPECT_EQ(l0[1].value, uint8_t(0xBB));
    auto l1 = hdma.run_line(1);
    { EXPECT_TRUE(l1.size() == 2); return; }
    EXPECT_EQ(l1[0].value, uint8_t(0xCC));
    EXPECT_EQ(l1[1].value, uint8_t(0xDD));
}

TEST(HdmaTiming, EffectsApplyAtLineStart) {
    // One fresh entry per line: value == line number + 1.
    std::array<uint8_t, 224 * 2> ram{};
    for (int n = 0; n < 224; ++n) {
        ram[size_t(n * 2)] = 0x01;
        ram[size_t(n * 2 + 1)] = uint8_t(n + 1);
    }
    auto hdma = make_engine(ram, {direct_channel(0x00)});
    hdma.init();
    // Line 4 must see value 5 DURING line 4 -- i.e. run_line(4) returns it.
    // Line 3's log must NOT contain it (that would be late application).
    for (int n : {0, 1, 2, 3}) {
        for (const auto& w : hdma.run_line(n)) {
            EXPECT_FALSE(w.value == uint8_t(5));
        }
    }
    auto l4 = hdma.run_line(4);
    { EXPECT_TRUE(l4.size() == 1); return; }
    EXPECT_EQ(l4[0].value, uint8_t(5));
}

TEST(HdmaInit, RestartsFrameFromTableStart) {
    const std::array<uint8_t, 4> ram{0x01, 0x42, 0x01, 0x43};
    auto hdma = make_engine(ram, {direct_channel(0x00)});
    hdma.init();
    auto first_pass = hdma.run_line(0);
    { EXPECT_TRUE(first_pass.size() == 1); return; }
    EXPECT_EQ(first_pass[0].value, uint8_t(0x42));
    hdma.init();  // next frame: rewind
    auto second_pass = hdma.run_line(0);
    { EXPECT_TRUE(second_pass.size() == 1); return; }
    EXPECT_EQ(second_pass[0].value, uint8_t(0x42));
}

TEST(HdmaChannels, OrderedAndIndependent) {
    // ch0 writes $2100, ch1 writes $2102; both fire on line 0, ch0 first.
    const std::array<uint8_t, 4> ram{0x01, 0xA0, 0x01, 0xB1};
    ChannelConfig c0 = direct_channel(0x00);
    ChannelConfig c1 = direct_channel(0x02);
    auto hdma = make_engine(ram, {c0, c1});
    hdma.init();
    auto l0 = hdma.run_line(0);
    { EXPECT_TRUE(l0.size() == 2); return; }
    EXPECT_EQ(l0[0].channel, 0);
    EXPECT_EQ(l0[0].value, uint8_t(0xA0));
    EXPECT_EQ(l0[1].reg, uint16_t(0x2102));
    EXPECT_EQ(l0[1].value, uint8_t(0xB1));
}
