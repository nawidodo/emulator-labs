#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "cpu.hpp"
#include "golden_trace.hpp"

using namespace nes6502;
using namespace ch19golden;

namespace {

// tests/public/ch19_nes_cpu_accuracy/programs/challenge_prog.bin — see the
// .asm.txt listing next to it for the disassembly and provenance. One
// contiguous image at $0600; the ADC/RTS subroutine copy lives at $0660
// inside it.
const uint8_t kProgram[] = {
    0x78, 0xA2, 0xFF, 0x9A, 0xA9, 0x90, 0x8D, 0xFE, 0xFF, 0xA9, 0x06, 0x8D,
    0xFF, 0xFF, 0x8D, 0xFA, 0xFF, 0x8D, 0xFB, 0xFF, 0x58, 0xA9, 0x01, 0x85,
    0x46, 0xA9, 0x10, 0x85, 0x47, 0xA9, 0x20, 0x85, 0x48, 0xA9, 0x80, 0x85,
    0x49, 0xA9, 0x42, 0x85, 0x40, 0xE6, 0x40, 0xA7, 0x40, 0x87, 0x41, 0xC7,
    0x46, 0xE7, 0x47, 0x07, 0x48, 0x27, 0x49, 0xA2, 0x02, 0x9D, 0xFE, 0x21,
    0xBD, 0xFF, 0x20, 0x20, 0x70, 0x06, 0x18, 0xA9, 0x80, 0x69, 0x80, 0x70,
    0x05, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0x00, 0xEA, 0xA0, 0x03, 0x88, 0xD0,
    0xFD, 0x4C, 0x59, 0x06, 0xEA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x18, 0xA9, 0x07, 0x69, 0x07, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xE6, 0x30, 0x40
};

constexpr uint16_t kHandlerAddr = 0x0700;
const uint8_t kIrqHandler[] = {0xE6, 0x30, 0x40};  // INC $30 ; RTI

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    Rig() {
        ram.mem[0xFFFE] = 0x00;
        ram.mem[0xFFFF] = 0x07;   // IRQ/BRK vector -> $0700
        ram.mem[0xFFFA] = 0x00;
        ram.mem[0xFFFB] = 0x07;   // NMI parked there too (never fires here)
        for (size_t i = 0; i < sizeof kIrqHandler; ++i)
            ram.mem[kHandlerAddr + i] = kIrqHandler[i];
        cpu.load_program(0x0600, kProgram, sizeof kProgram);
    }

    std::vector<std::string> logged_run(uint64_t max_steps) {
        std::vector<std::string> lines;
        while (!cpu.halted && lines.size() < max_steps) {
            const TraceRow row = peek_trace(ram, cpu.pc);
            step(cpu);
            lines.push_back(trace_line(cpu, row));
        }
        return lines;
    }
};

}  // namespace

TEST(challenge, final_machine_state) {
    Rig r;
    r.logged_run(100);

    EXPECT_EQ(r.ram.mem[0x30], 1);     // exactly one BRK serviced
    EXPECT_EQ(r.ram.mem[0x40], 0x43);  // INC'd zero-page cell
    EXPECT_EQ(r.ram.mem[0x41], 0x43);  // SAX stored A & X
    EXPECT_EQ(r.ram.mem[0x46], 0x00);  // DCP decremented past zero
    EXPECT_EQ(r.ram.mem[0x47], 0x11);  // ISB incremented
    EXPECT_EQ(r.ram.mem[0x48], 0x40);  // SLO shifted
    EXPECT_EQ(r.ram.mem[0x2200], 0x00);  // indexed store landed (A was $00)
    EXPECT_TRUE(r.cpu.halted);
}

TEST(challenge, trace_matches_reference_instruction_by_instruction) {
    Rig r;
    const std::vector<std::string> lines = r.logged_run(100);

    EXPECT_EQ(lines.size(), kGoldenTraceSize);
    const size_t n = lines.size() < kGoldenTraceSize ? lines.size()
                                                     : kGoldenTraceSize;
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
