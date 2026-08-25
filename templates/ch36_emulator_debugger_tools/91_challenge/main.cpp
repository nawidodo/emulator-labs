#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "session.hpp"

namespace {
fx8::Cpu make_cpu() {
    // 00: 01 05   LDA #5
    // 02: 04 01   ADD #1
    // 04: 03 20   STA $20
    // 06: 08 02   JZ $02
    // 08: 07 02   JMP $02
    // 0A: FF      HALT
    const std::vector<uint8_t> prog{0x01, 0x05, 0x04, 0x01, 0x03, 0x20,
                                    0x08, 0x02, 0x07, 0x02, 0xFF};
    fx8::Cpu cpu;
    cpu.load(prog);
    cpu.reset();
    return cpu;
}
}  // namespace

TEST(challenge, session_transcript_matches_golden) {
    auto cpu = make_cpu();
    challenge::DebugSession s(cpu);

    const std::vector<std::string> script = {
        "disasm pc", "bp 06", "run", "regs", "mem 20", "step", "quit"};
    std::vector<std::string> out;
    for (const auto& cmd : script) s.execute(cmd, out);

    // GOLDEN — regenerate via the reference solution if formats change.
    const char* golden =
        "LDA $05\n"
        "bp=06 set\n"
        "ran 3 steps\n"
        "{\"a\":6,\"x\":0,\"y\":0,\"pc\":6,\"z\":false,"
        "\"c\":false,\"cycles\":7}\n"
        "mem[20]=06\n"
        "pc=06 op=08 a=06 cyc=9\n"
        "bye\n";

    std::string joined;
    for (const auto& l : out) joined += l + "\n";
    EXPECT_EQ(joined, std::string(golden));
}

TEST(challenge, run_stops_at_breakpoint_only) {
    // No breakpoint can fire here (none set): run must stop on HALT.
    fx8::Cpu cpu;
    const std::vector<uint8_t> prog{0x01, 0x05, 0xFF};  // LDA #5; HALT
    cpu.load(prog);
    cpu.reset();
    challenge::DebugSession s(cpu);
    std::vector<std::string> out;
    s.execute("run", out);
    EXPECT_EQ(out.size(), size_t{1});
    EXPECT_EQ(out[0], "ran 2 steps");
}
