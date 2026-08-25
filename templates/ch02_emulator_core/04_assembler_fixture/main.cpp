#define LABSTEST_MAIN
#include "labstest.hpp"

#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "cpu.hpp"

namespace {

std::vector<uint8_t> read_program(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(trace, format_matches_canonical_line) {
    // The exact line from SPEC.md.
    const uint8_t regs[4] = {0x06, 0x01, 0x00, 0x00};
    EXPECT_EQ(ch02::format_trace(0x07, 0x50, regs, 4),
              "pc=07 op=50 r0=06 r1=01 r2=00 r3=00 cyc=4");
}

TEST(trace, traced_run_matches_step_by_step) {
    const uint8_t prog[] = {0x10, 0x00, 0x07, 0x50, 0x00, 0x00, 0x00};
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));

    std::vector<std::string> lines;
    while (!c.halted) {
        const uint8_t pre_pc = c.pc;
        const uint8_t op = c.ram[pre_pc];
        const uint8_t regs[4] = {c.r[0], c.r[1], c.r[2], c.r[3]};
        const ch02::StepResult res = c.step();
        lines.push_back(ch02::format_trace(pre_pc, op, regs, res.cycles));
    }
    EXPECT_EQ(lines.size(), 3u);
    // Trace shows PRE-execution state: first LOAD still sees r0 == 0.
    EXPECT_EQ(lines[0],
              "pc=00 op=10 r0=00 r1=00 r2=00 r3=00 cyc=4");
    EXPECT_EQ(lines[1],
              "pc=03 op=50 r0=07 r1=00 r2=00 r3=00 cyc=4");
}

TEST(fixture, countdown_bin_matches_asm_listing) {
    const auto prog = read_program("programs/countdown.bin");
    EXPECT_EQ(prog.size(), 16u);
    EXPECT_EQ(prog[0], 0x10);   // LOAD
    EXPECT_EQ(prog[6], 0x50);   // SUB at the loop head 0x06
    EXPECT_EQ(prog[9], 0x70);   // JZ
    EXPECT_EQ(prog[11], 0x60);  // JMP
    EXPECT_EQ(prog.back(), 0x00);
}

TEST(fixture, countdown_terminates_with_reference_cycle_count) {
    const auto prog = read_program("programs/countdown.bin");
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(prog.data(), prog.size()));
    const uint32_t spent = c.run(500);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.r[0], 0);
    EXPECT_EQ(spent, 73u);      // documented in countdown.asm.txt
}

TEST(fixture, sum_mem_writes_result_to_ram) {
    const auto prog = read_program("programs/sum_mem.bin");
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(prog.data(), prog.size()));
    (void)c.run(500);
    EXPECT_TRUE(c.halted);
    EXPECT_EQ(c.ram[0x80], 6);
    EXPECT_EQ(c.r[0], 6);
    EXPECT_EQ(c.r[1], 6);
}

TEST(disasm, samples) {
    const uint8_t prog[] = {0x10, 0x02, 0x07, 0x40, 0x01, 0x02,
                            0x60, 0x09, 0x99};
    ch02::Cpu c;
    c.load(std::span<const uint8_t>(prog, sizeof(prog)));
    EXPECT_EQ(ch02::disassemble(c, 0), "LOAD r2, #07");
    EXPECT_EQ(ch02::disassemble(c, 3), "ADD r1, r2");
    EXPECT_EQ(ch02::disassemble(c, 6), "JMP 0x09");
    EXPECT_EQ(ch02::disassemble(c, 8), ".byte 0x99");
}
