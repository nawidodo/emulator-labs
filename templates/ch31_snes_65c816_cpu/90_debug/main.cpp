#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "buggy_exec.hpp"

namespace {

std::string run_trace(snescpu::Cpu cpu, const std::vector<uint8_t>& prog) {
    snescpu::Mem mem;
    mem.load(0x00, 0x0000, prog.data(), prog.size());
    std::string out;
    while (cpu.cycles < 1000) {
        const uint16_t pc0 = cpu.pc;
        const uint8_t op = mem.read(cpu.k, cpu.pc);
        const int n = snescpu::step(cpu, mem);
        if (n < 0) break;
        cpu.cycles += static_cast<uint64_t>(n);
        out += snescpu::trace_line(cpu, pc0, op);
        out += '\n';
    }
    return out;
}

}  // namespace

// The seeded defect misreads the operand width of LDA #imm: it always
// consumes two bytes and clobbers the hidden high byte B. Both tests
// below pin down OBSERVABLE consequences of that defect.

TEST(debug, eight_bit_immediate_preserves_high_byte) {
    snescpu::Mem mem;
    snescpu::Cpu cpu;
    // Native mode, A forced 8-bit, then LDA #$78.
    const uint8_t prog[] = {0xE2, 0x20, 0xA9, 0x78, 0xEA, 0x00};
    mem.load(0x00, 0x0000, prog, sizeof(prog));
    cpu.e = false;
    cpu.p |= snescpu::FM;
    cpu.a = 0xABCD;
    const int n_sep = snescpu::step(cpu, mem);
    EXPECT_EQ(n_sep, 3);                                // SEP #$20
    const int n_lda = snescpu::step(cpu, mem);
    EXPECT_EQ(n_lda, 2);                                // LDA #$78
    EXPECT_EQ(cpu.a, 0xAB78);                           // B must survive
    const int n_nop = snescpu::step(cpu, mem);
    EXPECT_EQ(n_nop, 2);                                // NOP still there
}

TEST(debug, trace_matches_reference_execution) {
    snescpu::Cpu cpu;
    cpu.a = 0xABCD;
    const std::vector<uint8_t> prog = {
        0xFB,                 // XCE -> native
        0xC2, 0x30,           // REP #$30
        0xE2, 0x20,           // SEP #$20 (A 8-bit again)
        0xA9, 0x78,           // LDA #$78
        0x00,                 // BRK
    };
    const std::string got = run_trace(cpu, prog);
    const char* want =
        "pc=0000 op=FB k=00 db=00 a=ABCD x=0000 y=0000 p=35 sp=01FF cyc=2\n"
        "pc=0001 op=C2 k=00 db=00 a=ABCD x=0000 y=0000 p=05 sp=01FF cyc=5\n"
        "pc=0003 op=E2 k=00 db=00 a=ABCD x=0000 y=0000 p=25 sp=01FF cyc=8\n"
        "pc=0005 op=A9 k=00 db=00 a=AB78 x=0000 y=0000 p=25 sp=01FF "
        "cyc=10\n";
    EXPECT_EQ(got, want);
}
