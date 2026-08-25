#define LABSTEST_MAIN
#include <span>

#include "labstest.hpp"
#include "fixtures.hpp"
#include "machine.hpp"

namespace {

struct Result {
    uint16_t af, bc, de, hl, sp, pc;
    uint64_t cyc;
    bool halted;
    bool trap;
};

// Iteration cap keeps the skeleton build bounded: its step() stub neither
// advances cyc nor raises trap, so a pure cycle budget would spin forever.
constexpr int kMaxSteps = 2000000;

Result run(gb::Machine& m, uint64_t cycle_budget = 1000000) {
    for (int i = 0; i < kMaxSteps && m.cpu.cyc < cycle_budget; ++i) {
        if (!m.step()) break;
    }
    return {m.cpu.af(), m.cpu.bc(), m.cpu.de(), m.cpu.hl(), m.cpu.sp,
            m.cpu.pc,     m.cpu.cyc,  m.cpu.halted,  m.cpu.trap};
}

}  // namespace

// Program A: BCD quadrants + CB-page tour + stack/control flow. The golden
// final state below was produced by the reference implementation and is
// byte-stable across runs (see tests/public/.../goldens/goldens.md).
TEST(challenge, smoke_cpu_final_state_golden) {
    gb::Machine m;
    m.load(ch11_fixtures::smoke_cpu);
    const Result r = run(m);
    EXPECT_FALSE(r.trap);
    EXPECT_TRUE(r.halted);
    EXPECT_EQ(r.af, 0x0040);
    EXPECT_EQ(r.bc, 0x0700);
    EXPECT_EQ(r.de, 0x1433);
    EXPECT_EQ(r.hl, 0xFFFD);
    EXPECT_EQ(r.sp, 0xFFFA);
    EXPECT_EQ(r.pc, 0x0152);
    EXPECT_EQ(r.cyc, 1420);
}

// Program A left CB results in D/E and pushed BC through the stack; the
// stack bytes at $FFFC/$FFFD pin PUSH/POP behavior.
TEST(challenge, smoke_cpu_memory_and_stack_golden) {
    gb::Machine m;
    m.load(ch11_fixtures::smoke_cpu);
    run(m);
    EXPECT_EQ(m.bus.mem[0xFFFC], 0x00);
    EXPECT_EQ(m.bus.mem[0xFFFD], 0x07);
}

// Program B: EI-delay -> priority dispatch (VBlank before Timer) into the
// $0050 timer ISR twice, RETI between services, then WRAM snapshot.
TEST(challenge, smoke_irq_final_state_golden) {
    gb::Machine m;
    m.load(ch11_fixtures::smoke_irq);
    const Result r = run(m);
    EXPECT_FALSE(r.trap);
    EXPECT_TRUE(r.halted);              // asleep forever: nothing pending
    EXPECT_EQ(r.af, 0x0080);
    EXPECT_EQ(r.bc, 0x0000);
    EXPECT_EQ(r.sp, 0xFFFE);
    EXPECT_EQ(r.pc, 0x012B);
    EXPECT_EQ(r.cyc, 360);
    EXPECT_EQ(m.ctl.pending(), 0x00);   // both lines serviced
}

TEST(challenge, smoke_irq_service_counter_in_wram) {
    gb::Machine m;
    m.load(ch11_fixtures::smoke_irq);
    run(m);
    EXPECT_EQ(m.bus.mem[0xC100], 0x07);  // VBlank + Timer = 2
}
