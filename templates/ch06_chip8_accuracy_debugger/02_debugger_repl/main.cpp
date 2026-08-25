#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <sstream>
#include <string>

#include "debugger.hpp"

namespace {

using ch06::Chip8;

// Runs one scripted session against a fresh CPU and returns everything the
// debugger wrote to its output stream (prompts included).
std::string run_session(const uint8_t* rom, size_t len,
                        const std::string& script) {
    Chip8 cpu;
    cpu.load({rom, len});
    std::istringstream in(script);
    std::ostringstream out;
    ch06::Debugger dbg(cpu, in, out);
    dbg.set_prog_range(uint16_t(ch06::kProgBase + len));
    dbg.run();
    return out.str();
}

const uint8_t kRom[] = {
    0x60, 0x05,  // 200: LD   V0, 0x05
    0x61, 0x03,  // 202: LD   V1, 0x03
    0x80, 0x14,  // 204: ADD  V0, V1      -> V0 = 08
    0x70, 0x01,  // 206: ADD  V0, 0x01    -> V0 = 09
    0x81, 0x14,  // 208: ADD  V1, V1      -> V1 = 06
    0x00, 0x00,  // 20A: NOP
};

}  // namespace

TEST(repl, regs_dump_format) {
    const std::string out = run_session(kRom, sizeof kRom, "regs\nquit\n");
    EXPECT_EQ(out,
              "dbg> V0=00 V1=00 V2=00 V3=00\n"
              "V4=00 V5=00 V6=00 V7=00\n"
              "V8=00 V9=00 VA=00 VB=00\n"
              "VC=00 VD=00 VE=00 VF=00\n"
              "I=000 PC=0200 SP=00 DT=00 ST=00 CYC=0\n"
              "dbg> bye\n");
}

TEST(repl, memory_dump_rows) {
    const std::string out =
        run_session(kRom, sizeof kRom, "memory 200 6\nquit\n");
    EXPECT_EQ(out,
              "dbg> 0200: 60 05 61 03 80 14\n"
              "dbg> bye\n");
}

TEST(repl, step_prints_trace_lines) {
    const std::string out = run_session(kRom, sizeof kRom, "step 2\nquit\n");
    EXPECT_EQ(out,
              "dbg> pc=0200 op=6005 V0=00 I=000 SP=00 cyc=0\n"
              "pc=0202 op=6103 V0=05 I=000 SP=00 cyc=1\n"
              "dbg> bye\n");
}

TEST(repl, break_and_continue_hit) {
    const std::string out =
        run_session(kRom, sizeof kRom, "break 204\ncontinue\nregs\nquit\n");
    EXPECT_NE(out.find("breakpoint 0 set at 0204"), std::string::npos);
    EXPECT_NE(out.find("bp hit at 0204"), std::string::npos);
    // Registers were dumped right after the hit: V0 still 05, PC at 0204.
    EXPECT_NE(out.find("V0=05"), std::string::npos);
    EXPECT_NE(out.find("I=000 PC=0204 SP=00 DT=00 ST=00 CYC=2"),
              std::string::npos);
}

TEST(repl, continue_to_program_end) {
    const std::string out =
        run_session(kRom, sizeof kRom, "continue\nquit\n");
    EXPECT_NE(out.find("halted (pc=020C out of program)"),
              std::string::npos);
}

TEST(repl, disasm_from_pc) {
    const std::string out =
        run_session(kRom, sizeof kRom, "disasm 2\nquit\n");
    EXPECT_EQ(out,
              "dbg> 0200: 6005  LD V0, 0x05\n"
              "0202: 6103  LD V1, 0x03\n"
              "dbg> bye\n");
}

TEST(repl, unknown_command_and_help) {
    const std::string bad = run_session(kRom, sizeof kRom, "frobnicate\nquit\n");
    EXPECT_NE(bad.find("error: unknown command: frobnicate"),
              std::string::npos);
    const std::string help = run_session(kRom, sizeof kRom, "help\nquit\n");
    EXPECT_NE(help.find("step [n]"), std::string::npos);
    EXPECT_NE(help.find("memory <addr> [len]"), std::string::npos);
}

TEST(repl, eof_terminates_cleanly) {
    const std::string out = run_session(kRom, sizeof kRom, "");
    EXPECT_EQ(out, "dbg> bye\n");
}
