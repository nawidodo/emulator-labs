#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
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

TEST(cpudebug, disasm_is_pure_text) {
    // 00: 01 48  LDA #$48 / 02: 03 20  STA $20 / 04: FF HALT
    auto cpu = make_cpu({0x01, 0x48, 0x03, 0x20, 0xFF});
    dbg::Fx8Debug dbg(cpu);
    EXPECT_EQ(dbg.disasm(0), "LDA $48");
    EXPECT_EQ(dbg.disasm(2), "STA $20");
    EXPECT_EQ(dbg.disasm(4), "HALT");
}

TEST(cpudebug, disasm_ignores_live_state) {
    auto cpu = make_cpu({0x01, 0x10, 0x02, 0x30, 0xFF});
    dbg::Fx8Debug dbg(cpu);
    const std::string before = dbg.disasm(2);
    for (int i = 0; i < 3; ++i) dbg.step();  // machine moves elsewhere
    EXPECT_EQ(dbg.disasm(2), before);
}

TEST(cpudebug, step_reports_writes) {
    // 00: 01 99 LDA #$99 ; 02: 03 20 STA $20 ; 04: FF
    auto cpu = make_cpu({0x01, 0x99, 0x03, 0x20, 0xFF});
    dbg::Fx8Debug dbg(cpu);
    dbg.step();
    const auto info = dbg.step();
    EXPECT_EQ(info.pc_before, uint8_t{2});
    EXPECT_EQ(info.opcode, uint8_t{0x03});
    EXPECT_TRUE(info.writes.size() == 1);
    if (info.writes.size() == 1)
        EXPECT_EQ(info.writes[0], uint8_t{0x20});
}

TEST(cpudebug, breakpoint_counts_hits) {
    // pc0 LDA #0; then loop at 02..05: ADD #1; JMP $02.
    auto cpu = make_cpu({0x01, 0x00, 0x04, 0x01, 0x07, 0x02});
    dbg::Fx8Debug dbg(cpu);
    dbg.add_breakpoint(2);
    int fires = 0;
    for (int i = 0; i < 40 && fires < 3; ++i) {
        dbg.step();
        if (dbg.check_breakpoints()) ++fires;
    }
    EXPECT_EQ(fires, 3);
    EXPECT_EQ(dbg.breakpoint_hits(2), 3);
}

TEST(cpudebug, watchpoint_tracks_value_change) {
    // 00: 01 42 LDA #$42 ; 02: 03 20 STA $20 ; 04: FF
    auto cpu = make_cpu({0x01, 0x42, 0x03, 0x20, 0xFF});
    dbg::Fx8Debug dbg(cpu);
    dbg.add_watchpoint(0x20);
    dbg.step();
    EXPECT_EQ(dbg.watch_hits(0x20), 0);  // not written yet
    dbg.step();
    EXPECT_EQ(dbg.watch_hits(0x20), 1);
    EXPECT_EQ(dbg.last_written(0x20), uint8_t{0x42});
}

TEST(cpudebug, regs_json_exact_shape) {
    auto cpu = make_cpu({0xFF});
    cpu.a = 5;
    cpu.z = true;
    dbg::Fx8Debug dbg(cpu);
    EXPECT_EQ(dbg.regs_json(),
              "{\"a\":5,\"x\":0,\"y\":0,\"pc\":0,\"z\":true,"
              "\"c\":false,\"cycles\":0}");
}
