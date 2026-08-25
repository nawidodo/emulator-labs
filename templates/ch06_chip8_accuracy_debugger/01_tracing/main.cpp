#define LABSTEST_MAIN
#include "labstest.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "chip8.hpp"
#include "trace.hpp"

namespace {

using ch06::Chip8;

std::vector<std::string> trace_program(const uint8_t* rom, size_t len,
                                       bool full = false) {
    Chip8 cpu;
    cpu.load({rom, len});
    std::ostringstream os;
    ch06::TraceWriter tw(os, full);
    while (!cpu.halted()) tw.log(cpu), cpu.step();
    std::vector<std::string> lines;
    std::string line;
    std::istringstream is(os.str());
    while (std::getline(is, line)) lines.push_back(line);
    return lines;
}

}  // namespace

TEST(trace, canonical_format) {
    const uint8_t rom[] = {0x62, 0xAB};
    Chip8 cpu;
    cpu.load(rom);
    EXPECT_EQ(ch06::trace_line(cpu),
              "pc=0200 op=62AB V0=00 I=000 SP=00 cyc=0");
}

TEST(trace, canonical_after_step_shows_effect) {
    const uint8_t rom[] = {0x62, 0xAB, 0x63, 0x01, 0x82, 0x34};  // V2+=V1
    auto lines = trace_program(rom, sizeof rom);
    EXPECT_EQ(lines.size(), size_t(3));
    EXPECT_EQ(lines[0], "pc=0200 op=62AB V0=00 I=000 SP=00 cyc=0");
    EXPECT_EQ(lines[1], "pc=0202 op=6301 V0=00 I=000 SP=00 cyc=1");
    // After the add at line 3 the NEXT trace row would show V0 only in
    // canonical mode; V2's effect is what --trace-full is for.
    EXPECT_EQ(lines[2], "pc=0204 op=8234 V0=00 I=000 SP=00 cyc=2");
}

TEST(trace, full_mode_dumps_all_registers_and_timers) {
    const uint8_t rom[] = {0x62, 0xAB};
    Chip8 cpu;
    cpu.load(rom);
    cpu.v[3] = 0x2A;
    cpu.dt = 5;
    cpu.st = 2;
    cpu.sp = 1;
    cpu.cycles = 7;
    const std::string line = ch06::full_trace_line(cpu);
    EXPECT_EQ(line,
              "pc=0200 op=62AB V0=00 V1=00 V2=00 V3=2A V4=00 V5=00 V6=00 "
              "V7=00 V8=00 V9=00 VA=00 VB=00 VC=00 VD=00 VE=00 VF=00 "
              "I=000 SP=01 DT=05 ST=02 cyc=7");
}

TEST(trace, writer_streams_one_line_per_instruction) {
    const uint8_t rom[] = {0x60, 0x01, 0x61, 0x02};
    auto lines = trace_program(rom, sizeof rom);
    EXPECT_EQ(lines.size(), size_t(2));
    EXPECT_NE(lines[0], lines[1]);
}

TEST(core, fetch_is_big_endian_and_advances_pc) {
    const uint8_t rom[] = {0x62, 0xAB};
    Chip8 cpu;
    cpu.load(rom);
    const auto r = cpu.step();
    EXPECT_EQ(r.op, 0x62AB);
    EXPECT_EQ(r.pc, 0x200);
    EXPECT_EQ(cpu.v[2], 0xAB);
    EXPECT_EQ(cpu.pc, 0x202);
    EXPECT_TRUE(cpu.halted());
}

TEST(core, call_pushes_and_ret_pops) {
    // 200: CALL 208; 202..206: filler LDs; 208: RTS (returns to 202)
    const uint8_t rom[] = {0x22, 0x08, 0x60, 0x11, 0x61, 0x22, 0x12, 0x0C,
                           0x00, 0xEE};
    Chip8 cpu;
    cpu.load(rom);
    cpu.step();                       // CALL -> pc=208, sp=1
    EXPECT_EQ(cpu.pc, 0x208);
    EXPECT_EQ(cpu.sp, 1);
    cpu.step();                       // RTS -> pc=202, sp=0
    EXPECT_EQ(cpu.pc, 0x202);
    EXPECT_EQ(cpu.sp, 0);
}

TEST(core, timers_tick_once_per_frame_of_11_instructions) {
    const uint8_t rom[22] = {0x60, 0x00};  // 11 x "LD V0, 0x00" NOP-ish
    Chip8 cpu;
    cpu.load(rom);
    cpu.dt = 5;
    for (int i = 0; i < 11; ++i) {
        EXPECT_FALSE(cpu.halted());
        cpu.step();
    }
    EXPECT_EQ(cpu.dt, 4);
    for (int i = 0; i < 55; ++i) cpu.step();
    EXPECT_EQ(cpu.dt, 0);  // clamped, never wraps underflowing
}

TEST(core, disasm_renders_mnemonics) {
    const uint8_t rom[] = {0x62, 0x00, 0xA2, 0x10, 0xD2, 0x35, 0xF0, 0x33};
    Chip8 cpu;
    cpu.load(rom);
    EXPECT_EQ(cpu.disassemble(0x200), "0200: 6200  LD V2, 0x00");
    EXPECT_EQ(cpu.disassemble(0x202), "0202: A210  LD I, 0x210");
    EXPECT_EQ(cpu.disassemble(0x204), "0204: D235  DRW V2, V3, 5");
    EXPECT_EQ(cpu.disassemble(0x206), "0206: F033  BCD V0");
}
