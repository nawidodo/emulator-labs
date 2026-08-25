#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "cpudebug.hpp"

namespace {
fx8::Cpu make_cpu(const std::vector<uint8_t>& prog) {
    fx8::Cpu cpu;
    cpu.load(prog);
    cpu.reset();
    return cpu;
}
}  // namespace

// pc path: 00 LDA #0 -> 02 ADD #1 -> 04 ADD #1 -> 06 JMP $02 ...
TEST(debug_bp, breakpoint_counts_hits) {
    auto cpu = make_cpu({0x01, 0x00, 0x04, 0x01, 0x04, 0x01, 0x07, 0x02});
    dbg::Fx8Debug dbg(cpu);
    dbg.add_breakpoint(2);
    int fires = 0;
    for (int i = 0; i < 60 && fires < 4; ++i) {
        dbg.step();
        if (dbg.check_breakpoints()) ++fires;
    }
    // RED with the seeded bug: masked compare also "hits" at pc=3.
    EXPECT_EQ(fires, 4);
    EXPECT_EQ(dbg.breakpoint_hits(2), 4);
}

TEST(debug_bp, odd_breakpoint_fires_exactly) {
    auto cpu = make_cpu({0x01, 0x00, 0x04, 0x01, 0x07, 0x03});  // loop at 03
    dbg::Fx8Debug dbg(cpu);
    dbg.add_breakpoint(3);  // odd address
    int fires = 0;
    for (int i = 0; i < 40 && fires < 2; ++i) {
        dbg.step();
        if (dbg.check_breakpoints()) ++fires;
    }
    EXPECT_EQ(fires, 2);
}

TEST(debug_bp, other_tooling_still_works) {
    auto cpu = make_cpu({0x01, 0x42, 0xFF});
    dbg::Fx8Debug dbg(cpu);
    dbg.step();
    EXPECT_EQ(dbg.disasm(0), "LDA $42");
    EXPECT_NE(dbg.regs_json().find("\"a\":66"), std::string::npos);
}
