#define LABSTEST_MAIN
#include "labstest.hpp"

#include "frame_machine.hpp"

#include <sstream>
#include <string>
#include <vector>

// Interrupt-cadence contract tests.
//
// Unit tests drive VblankTimers at test scale (a few hundred cycles per
// frame). The integration tests run the committed cadence program — a
// delay loop with counting RST 08 / RST 10 handlers — and check both the
// counter values in RAM and the cycle windows the one-shots fired in.

using namespace si;

namespace {

// The ch09_cadence fixture (tests/public/ch09_space_invaders_machine/roms,
// listing in the adjacent .asm.txt): the mainline burns ~163k T-states,
// the RST 08 handler bumps the word at 0x2000, RST 10 the word at 0x2002.
const uint8_t kCadenceProgram[] = {
        0xC3, 0x40, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x50, 0x00, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xC3, 0x5D, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xF3, 0x31, 0x00, 0x21, 0xFB, 0x01, 0x98, 0x1A,
        0x0B, 0x78, 0xB1, 0xC2, 0x48, 0x00, 0xF3, 0x76, 0xF5, 0xE5, 0x2A, 0x00,
        0x20, 0x23, 0x22, 0x00, 0x20, 0xE1, 0xF1, 0xFB, 0xC9, 0xF5, 0xE5, 0x2A,
        0x02, 0x20, 0x23, 0x22, 0x02, 0x20, 0xE1, 0xF1, 0xFB, 0xC9,
};

std::vector<uint8_t> cadence_program() {
    return std::vector<uint8_t>(kCadenceProgram,
                                kCadenceProgram + sizeof kCadenceProgram);
}

}  // namespace

TEST(timers, fire_once_per_period_starting_with_even_opcode) {
    VblankTimers t;
    t.configure(100, kIrqOpcodeEven, kIrqOpcodeOdd);
    EXPECT_EQ(t.poll(50).raised, false);      // before the first deadline
    IrqRaise r = t.poll(100);
    EXPECT_TRUE(r.raised);
    EXPECT_EQ(r.opcode, kIrqOpcodeEven);      // even frame: RST 08
    EXPECT_EQ(t.next_fire(), 200u);

    r = t.poll(199);
    EXPECT_FALSE(r.raised);                   // not yet again
    r = t.poll(200);
    EXPECT_TRUE(r.raised);
    EXPECT_EQ(r.opcode, kIrqOpcodeOdd);       // odd frame: RST 10
}

TEST(timers, alternation_is_stable_over_many_frames) {
    VblankTimers t;
    t.configure(64, 0xCF, 0xD7);
    for (unsigned frame = 0; frame < 16; ++frame) {
        const IrqRaise r = t.poll(uint64_t(frame + 1) * 64);
        EXPECT_TRUE(r.raised);
        EXPECT_EQ(r.opcode, (frame % 2 == 0) ? 0xCF : 0xD7);
    }
}

TEST(timers, configure_resets_parity_and_deadline) {
    VblankTimers t;
    t.configure(10, 0xCF, 0xD7);
    t.poll(30);                               // two frames consumed
    EXPECT_EQ(t.even_frame(), false);
    t.configure(10, 0xCF, 0xD7);              // re-arm like a fresh board
    EXPECT_EQ(t.even_frame(), true);
    EXPECT_EQ(t.next_fire(), 10u);
}

TEST(integration, cadence_program_counts_alternating_vectors) {
    FrameMachine m;
    m.load(cadence_program());
    // Seven frame periods budgeted: boundaries at 32k, 64k, ... The
    // program halts at ~163k, so exactly five boundaries pass:
    //   RST 08 @32k, RST 10 @64k, RST 08 @96k, RST 10 @128k, RST 08 @160k.
    m.run(7 * kCyclesPerFrame, nullptr);
    EXPECT_TRUE(m.cpu().halted);
    EXPECT_EQ(m.raises(), 5);
    EXPECT_EQ(m.accepts(), 5);                // handlers EI before RET
    const int cnt8 = m.read(0x2000) | (m.read(0x2001) << 8);
    const int cnt10 = m.read(0x2002) | (m.read(0x2003) << 8);
    EXPECT_EQ(cnt8, 3);
    EXPECT_EQ(cnt10, 2);
    EXPECT_EQ(m.cpu().iff, false);            // mainline ends with DI; HLT
}

TEST(integration, raise_cycles_land_in_boundary_windows) {
    // A one-shot cannot fire mid-instruction: the raise surfaces at the
    // first poll AFTER the boundary, so each observed cycle sits in
    // [n*frame, n*frame + max_step_cost).
    FrameMachine m;
    m.load(cadence_program());
    m.run(6 * kCyclesPerFrame, nullptr);
    EXPECT_EQ(m.raise_cycles().size(), 5u);
    for (size_t i = 0; i < m.raise_cycles().size(); ++i) {
        const uint64_t lo = (i + 1) * kCyclesPerFrame;
        const uint64_t hi = lo + 32;          // longest step here < 32T
        EXPECT_TRUE(m.raise_cycles()[i] >= lo);
        EXPECT_TRUE(m.raise_cycles()[i] < hi);
    }
}

TEST(integration, masked_cpu_loses_the_pulse) {
    // DI from the very start and never EI again: every raise must be
    // rejected. Edge one-shots do not latch — a masked CPU just misses it.
    FrameMachine m;
    std::vector<uint8_t> prog = {0xF3};       // DI
    prog.insert(prog.end(), {0x01, 0xE8, 0x03});   // LXI B,1000
    for (int i = 0; i < 400; ++i) prog.push_back(0x00);   // NOP sled
    prog.push_back(0x76);                     // HLT
    m.configure_timing(500, 0xCF, 0xD7);      // fast frames so raises happen
    m.load(prog);
    m.run(6000, nullptr);
    EXPECT_TRUE(m.raises() >= 1);
    EXPECT_EQ(m.accepts(), 0);
}

TEST(trace, canonical_lines_are_stable) {
    // A tiny deterministic run observed through the trace: the first line
    // is always the JMP fetch at reset.
    FrameMachine m;
    m.load(cadence_program());
    std::ostringstream tr;
    m.run(40, &tr);
    const std::string lines = tr.str();
    EXPECT_EQ(lines.substr(0, 13), "pc=0000 op=C3");
}
