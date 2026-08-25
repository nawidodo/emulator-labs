#define LABSTEST_MAIN
#include "labstest.hpp"

#include <sstream>
#include <string>

#include "debugger.hpp"
#include "watchpoint.hpp"

namespace {

using ch06::Chip8;

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

// Stores V3 = 0x2A into memory at 0x400, then increments V3 by one.
const uint8_t kRom[] = {
    0x63, 0x2A,  // 200: LD   V3, 0x2A
    0xA4, 0x00,  // 202: LD   I, 0x400
    0xF3, 0x55,  // 204: LD   [I], V3      -> mem[403] = 2A (V0..V3 stored)
    0x73, 0x01,  // 206: ADD  V3, 0x01     -> V3 = 2B
    0x00, 0x00,  // 208: NOP
};

}  // namespace

TEST(watchparse, mem_range) {
    std::istringstream is("400:4");
    auto w = ch06::parse_watch_spec(is);
    EXPECT_TRUE(w.has_value());
    EXPECT_TRUE(w->is_mem);
    EXPECT_EQ(w->addr, 0x400);
    EXPECT_EQ(w->len, 4);

    std::istringstream bare("401");
    w = ch06::parse_watch_spec(bare);
    EXPECT_TRUE(w.has_value());
    EXPECT_EQ(w->addr, 0x401);
    EXPECT_EQ(w->len, 1);
}

TEST(watchparse, register_predicate) {
    std::istringstream is("V3==2A");
    auto w = ch06::parse_watch_spec(is);
    EXPECT_TRUE(w.has_value());
    EXPECT_FALSE(w->is_mem);
    EXPECT_EQ(w->reg, 3);
    EXPECT_EQ(w->value, 0x2A);

    for (const char* spec : {"V3", "V==2A", "V9%11", "3=FF"}) {
        std::istringstream bad(spec);
        EXPECT_FALSE(ch06::parse_watch_spec(bad).has_value());
    }
}

TEST(watchset, memory_range_hit) {
    Chip8 cpu;
    cpu.load(kRom);
    ch06::WatchSet ws;
    ch06::WatchSpec w;
    w.is_mem = true;
    w.addr = 0x400;
    w.len = 4;
    ws.add(w, cpu);
    const std::string before = ws.check(cpu);
    EXPECT_TRUE(before.empty());
    while (cpu.pc != 0x206 && !cpu.halted()) cpu.step();
    const std::string hit = ws.check(cpu);
    EXPECT_NE(hit.find("mem[0403] changed 00->2A"), std::string::npos);
    EXPECT_TRUE(ws.check(cpu).empty());  // snapshot refreshed: fires once
}

TEST(repl, register_predicate_session) {
    const std::string out = run_session(kRom, sizeof kRom,
                                        "watch V3>=2B\ncontinue\nquit\n");
    EXPECT_NE(out.find("watchpoint 0 added"), std::string::npos);
    EXPECT_NE(out.find("watch hit: V3=2B satisfies V3>=2B at pc=0208"),
              std::string::npos);
}

TEST(repl, memory_range_session) {
    const std::string out =
        run_session(kRom, sizeof kRom, "watch 400:4\ncontinue\nquit\n");
    EXPECT_NE(out.find("mem[0403] changed 00->2A"), std::string::npos);
}

TEST(repl, watch_bad_spec_is_an_error) {
    const std::string out =
        run_session(kRom, sizeof kRom, "watch nonsense\nquit\n");
    EXPECT_NE(out.find("error: bad arguments"), std::string::npos);
}
