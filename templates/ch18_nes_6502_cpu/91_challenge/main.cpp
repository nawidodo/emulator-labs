#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "golden_trace.hpp"

using namespace nes6502;

namespace {

// tests/public/ch18_nes_6502_cpu/programs/challenge_prog.bin — see the
// .asm.txt listing next to it for the disassembly and provenance.
const uint8_t kProgram[] = {
    0xA9, 0xFF, 0x85, 0x40, 0xE6, 0x40, 0xA9, 0x05, 0x4A, 0xD0, 0xFD, 0x69,
    0x30, 0x8D, 0x10, 0x02, 0xA2, 0x02, 0xBD, 0xFE, 0x20, 0x20, 0x40, 0x06,
    0xAA, 0x8A, 0x0A, 0x8D, 0x11, 0x02, 0x18, 0xA9, 0x80, 0x69, 0x80, 0x70,
    0x02, 0xA9, 0xEE, 0x8D, 0x12, 0x02, 0x6C, 0x50, 0x06, 0x00, 0x00, 0x00,
    0xEA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x69, 0x07, 0x60, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x06
};

}  // namespace

TEST(challenge, final_machine_state) {
    FlatRam ram;
    Cpu cpu{.bus = &ram};
    ram.mem[0x2100] = 0x0F;  // preloaded data page
    cpu.load_program(0x0600, kProgram, sizeof kProgram);
    run(cpu, 1000);
    EXPECT_EQ(ram.mem[0x0040], 0x00);
    EXPECT_EQ(ram.mem[0x0210], 0x31);
    EXPECT_EQ(ram.mem[0x0211], 0x2C);
    EXPECT_EQ(ram.mem[0x0212], 0x00);
    EXPECT_TRUE(cpu.halted);
}

TEST(challenge, trace_matches_reference_instruction_by_instruction) {
    FlatRam ram;
    Cpu cpu{.bus = &ram};
    ram.mem[0x2100] = 0x0F;
    cpu.load_program(0x0600, kProgram, sizeof kProgram);

    std::vector<std::string> lines;
    while (!cpu.halted && lines.size() < 100) {
        const uint16_t pc_before = cpu.pc;
        const uint8_t op = ram.mem[pc_before];
        step(cpu);
        char buf[96];
        std::snprintf(buf, sizeof buf,
                      "pc=%04x op=%02x a=%02x x=%02x y=%02x p=%02x sp=%02x "
                      "cyc=%llu",
                      pc_before, op, cpu.a, cpu.x, cpu.y, cpu.p, cpu.sp,
                      (unsigned long long)cpu.cycles);
        lines.emplace_back(buf);
    }

    EXPECT_EQ(lines.size(), kGoldenTrace.size());
    const size_t n = lines.size() < kGoldenTrace.size() ? lines.size()
                                                        : kGoldenTrace.size();
    for (size_t i = 0; i < n; ++i) {
        if (lines[i] != kGoldenTrace[i]) {
            std::printf(
                "first divergence at line %zu:\n  got:  %s\n  want: %s\n",
                i + 1, lines[i].c_str(), kGoldenTrace[i].c_str());
            EXPECT_EQ(lines[i], kGoldenTrace[i]);
            break;
        }
    }
}
