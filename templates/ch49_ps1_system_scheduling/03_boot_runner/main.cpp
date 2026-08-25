#define LABSTEST_MAIN
#include "labstest.hpp"
#include "../02_mini_devices/system.hpp"

#include <sstream>
#include <string>
#include <vector>

using namespace ps1sys;

namespace {

// A tiny boot program: milestone 1, SPU on, CD read, spin on I_STAT bit2,
// ack, milestone 2, HALT. Exercises the same wiring as the committed
// fixtures without depending on files at test runtime.
template <class... W>
std::vector<uint32_t> program(W... words) {
    return std::vector<uint32_t>{words...};
}

uint32_t lui(unsigned rt, uint16_t imm) { return rt << 16 | imm; }
uint32_t addiu(unsigned rt, unsigned rs, uint16_t imm) {
    return 2u << 26 | rs << 21 | rt << 16 | imm;
}
uint32_t sw(unsigned rt, uint16_t off, unsigned rs) {
    return 3u << 26 | rs << 21 | rt << 16 | off;
}
uint32_t lw(unsigned rt, uint16_t off, unsigned rs) {
    return 4u << 26 | rs << 21 | rt << 16 | off;
}
uint32_t halt() { return 7u << 26; }

struct RunResult {
    Log event_log;
    std::vector<std::string> trace;
    bool halted;
};

RunResult run_program(const std::vector<uint32_t>& prog,
                      const std::string& script_text = "") {
    sched::Scheduler sch;
    System<sched::Scheduler> sys(sch);
    sys.reset();
    sys.load_rom(reinterpret_cast<const uint8_t*>(prog.data()),
                 prog.size() * 4);
    if (!script_text.empty()) {
        std::istringstream ss(script_text);
        EXPECT_TRUE(sys.apply_script(ss));
    }
    sys.run_until(100000);
    RunResult r;
    r.event_log = sys.event_log();
    r.trace = sys.trace();
    r.halted = sys.halted();
    return r;
}

}  // namespace

TEST(runner, event_log_is_deterministic_across_runs) {
    const auto prog = program(
        lui(8, 0x1F80), addiu(9, 0, 1), sw(9, 0xFF0, 8), halt());
    const auto a = run_program(prog);
    const auto b = run_program(prog);
    EXPECT_EQ(a.event_log.size(), b.event_log.size());
    for (size_t i = 0; i < a.event_log.size() && i < b.event_log.size(); ++i)
        EXPECT_EQ(a.event_log[i], b.event_log[i]);
    EXPECT_TRUE(a.halted);
}

TEST(runner, event_log_line_shape_is_pinned) {
    const auto prog = program(lui(8, 0x1F80), addiu(12, 0, 7),
                              sw(12, 0xFF0, 8), halt());
    const auto r = run_program(prog);
    EXPECT_TRUE(r.event_log.size() >= 3);
    if (r.event_log.size() >= 3) {
        EXPECT_EQ(r.event_log.front(), std::string("cyc=0 evt=boot"));
        EXPECT_EQ(r.event_log[1], std::string("cyc=8 evt=milestone val=7"));
        EXPECT_EQ(r.event_log.back(), std::string("cyc=12 evt=halt"));
    }
}

TEST(runner, trace_lines_follow_canonical_shape) {
    const auto prog = program(addiu(1, 0, 5), addiu(2, 1, 3),
                              sw(2, 0x40, 0), lw(3, 0x40, 0), halt());
    const auto r = run_program(prog);
    EXPECT_EQ(r.trace.size(), size_t(5));
    // pc is the BYTE address of the instruction, op its raw word.
    if (r.trace.size() == 5) {
        EXPECT_EQ(r.trace[0], std::string("pc=00000000 op=08010005 cyc=0"));
        EXPECT_EQ(r.trace[1], std::string("pc=00000004 op=08220003 cyc=4"));
        EXPECT_EQ(r.trace[4], std::string("pc=00000010 op=1C000000 cyc=16"));
    }
}

TEST(runner, script_dma_kick_stalls_boot_until_drain) {
    // Script kicks a 4-word DMA at cycle 0: the bus is already owned when
    // the first CPU event fires, so the core stalls BEFORE its first
    // fetch and only resumes once the drain completes (24 cycles in).
    const auto prog = program(addiu(1, 0, 1), addiu(2, 0, 2),
                              addiu(3, 0, 3), halt());
    const auto r = run_program(prog, "# script kick\nDMA 4\n");
    EXPECT_EQ(r.trace.size(), size_t(4));
    if (r.trace.size() == 4) {
        EXPECT_EQ(r.trace[0], std::string("pc=00000000 op=08010001 cyc=24"));
        EXPECT_EQ(r.trace[1], std::string("pc=00000004 op=08020002 cyc=28"));
        EXPECT_EQ(r.trace[3], std::string("pc=0000000C op=1C000000 cyc=36"));
    }
    bool saw_start = false, saw_done = false;
    for (const auto& line : r.event_log) {
        if (line.find("cyc=0 evt=dma_start words=4") != std::string::npos)
            saw_start = true;
        if (line.find("cyc=24 evt=dma_done words=4") != std::string::npos)
            saw_done = true;
    }
    EXPECT_TRUE(saw_start);
    EXPECT_TRUE(saw_done);
}
