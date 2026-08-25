#define LABSTEST_MAIN
#include "labstest.hpp"
#include <string>
#include "machine.hpp"
#include "scheduler.hpp"
#include <cstddef>

using ps1::sysdev::Machine;
using ps1::sysdev::Scheduler;

namespace {

// Records (cycle, dispatch-order) pairs as events fire.
struct Recorder {
    Scheduler* sched = nullptr;
    std::vector<std::pair<uint64_t, int>> fired;

    static void fire(void* self) {
        auto* r = static_cast<Recorder*>(self);
        r->fired.emplace_back(r->sched->now(),
                              static_cast<int>(r->fired.size()));
    }
};

constexpr uint32_t kSched0[] = {
    // poll I_STAT until Timer0 raises bit 4, record + acknowledge
    0x3C081F80u, 0x35081100u, 0x3409003Cu, 0xAD090008u,  // t0 base, target=60
    0x34090018u, 0xAD090004u,                            // mode=reset@tgt|irq@tgt
    0x3C0B1F80u, 0x356B1070u,                            // t3 = I_STAT
    0x8D6A0000u,                                         // poll: lw t2,0(t3)
    0x1140FFFEu, 0x00000000u,                            // beq t2,zero,poll ; nop
    0xAC0A0200u,                                         // sw t2, 200h($0)
    0xAD6A0000u,                                         // ack: sw t2,0(t3)
    0x8D0D0000u, 0xAC0D0204u,                            // sample counter -> 204h
    0x1000FFFFu, 0x00000000u,                            // spin: beq $0,$0,-1
};

}  // namespace

TEST(scheduler_order, fires_due_events_oldest_first) {
    Scheduler s;
    Recorder r;
    r.sched = &s;
    s.schedule(30, 1, &Recorder::fire, &r);
    s.schedule(10, 2, &Recorder::fire, &r);
    s.schedule(20, 3, &Recorder::fire, &r);
    s.run_to(25);
    EXPECT_EQ(r.fired.size(), 2u);
    EXPECT_EQ(r.fired[0].first, 10u);
    EXPECT_EQ(r.fired[1].first, 20u);
}

TEST(scheduler_order, same_cycle_fifo_by_insertion) {
    Scheduler s;
    Recorder r;
    r.sched = &s;
    s.schedule(50, 7, &Recorder::fire, &r);
    s.schedule(50, 8, &Recorder::fire, &r);
    s.run_to(50);
    EXPECT_EQ(r.fired.size(), 2u);
    EXPECT_TRUE(r.fired[0].second <= r.fired[1].second);
    EXPECT_EQ(s.now(), 50u);
}

TEST(scheduler_cancel, removes_pending_events) {
    Scheduler s;
    Recorder r;
    r.sched = &s;
    s.schedule(10, 42, &Recorder::fire, &r);
    s.schedule(20, 43, &Recorder::fire, &r);
    EXPECT_TRUE(s.cancel(42));
    EXPECT_FALSE(s.cancel(42));                 // already gone
    s.run_to(100);
    EXPECT_EQ(r.fired.size(), 1u);
    EXPECT_EQ(r.fired[0].first, 20u);
    EXPECT_EQ(s.pending(), 0u);
}

TEST(scheduler_resched, event_firing_can_schedule_more_work_in_time) {
    struct Chain {
        Scheduler* s;
        int depth = 0;
        std::vector<uint64_t> at;
        static void fire(void* self) {
            auto* c = static_cast<Chain*>(self);
            c->at.push_back(c->s->now());
            if (++c->depth < 3) c->s->schedule(c->s->now() + 5, 9, &fire, self);
        }
    };
    Scheduler s;
    Chain c{&s, 0, {}};
    s.schedule(4, 9, &Chain::fire, &c);
    s.run_to(14);                                // chain: 4, 9, 14 all due
    EXPECT_EQ(c.at.size(), 3u);                  // last link fires inside call
    EXPECT_EQ(c.at[2], 14u);
}

TEST(machine_bus, ram_roundtrip_and_mirrors) {
    Machine m;
    m.write32(0x80010000u, 0xDEADBEEFu);         // kseg0
    EXPECT_EQ(m.read32(0x80010000u), 0xDEADBEEFu);
    EXPECT_EQ(m.read32(0x00010000u), 0xDEADBEEFu);   // kuseg mirror
    EXPECT_EQ(m.read32(0xA0010000u), 0xDEADBEEFu);   // kseg1 mirror
    m.write32(0x00300000u, 0x12345678u);         // beyond RAM: ignored
    EXPECT_EQ(m.read32(0x00300000u), 0xFFFFFFFFu);
}

TEST(machine_bus, irq_registers_decode) {
    Machine m;
    m.write32(0x1F801074u, ps1::sysdev::kIrqVblank | ps1::sysdev::kIrqTimer0);
    EXPECT_EQ(m.read32(0x1F801074u),
              (ps1::sysdev::kIrqVblank | ps1::sysdev::kIrqTimer0));
    m.timers.regs[0].counter = 0x1234u;
    EXPECT_EQ(m.read32(0x1F801100u), 0x1234u);       // T0 COUNTER
    EXPECT_EQ(m.read32(0x1F801108u), 0u);            // T0 TARGET
    EXPECT_EQ(m.read32(0x1F801110u), 0u);            // T1 COUNTER
    // I_STAT write acknowledges with write-1-clears.
    m.irq.raise(ps1::sysdev::kIrqTimer0 | ps1::sysdev::kIrqVblank);
    m.irq.lower(ps1::sysdev::kIrqTimer0 | ps1::sysdev::kIrqVblank);
    m.write32(0x1F801070u, ps1::sysdev::kIrqTimer0);
    EXPECT_EQ(m.read32(0x1F801070u), ps1::sysdev::kIrqVblank);
}

TEST(machine_cpu, lui_ori_sw_lw_roundtrip) {
    Machine m;
    static const uint32_t prog[] = {
        0x3C081234u,             // lui $t0, 0x1234
        0x35085678u,             // ori $t0, $t0, 0x5678
        0xAC080300u,             // sw $t0, 0x300($zero)
        0x8C090304u,             // lw $t1, 0x304($zero)
    };
    m.load_program(prog, 4);
    m.run_until(8);
    EXPECT_EQ(m.ram[0x300], 0x78u);              // low byte of 12345678h
    EXPECT_EQ(m.cpu.regs[9], 0u);                // read of empty word
}

TEST(machine_cpu, branch_delay_slot_executes) {
    Machine m;
    static const uint32_t prog[] = {
        0x24090002u,             // addiu $t1, $zero, 2      @ 80010000
        0x10000002u,             // beq $zero,$zero,+2       @ 80010004
        0x240A0007u,             // delay: addiu $t2, $zero, 7
        0x240B0009u,             // skipped: addiu $t3, $zero, 9
        0x240C0001u,             // land here: addiu $t4, $zero, 1
        0x1000FFFFu, 0x00000000u,                    // spin
    };
    m.load_program(prog, 7);
    m.run_until(16);
    EXPECT_EQ(m.cpu.regs[10], 7u);               // delay slot ran
    EXPECT_EQ(m.cpu.regs[11], 0u);               // branch target skipped it
    EXPECT_EQ(m.cpu.pc, 0x80010014u);            // parked in the spin loop
}

TEST(machine_integration, timer_irq_reaches_polled_program) {
    Machine m;
    std::string trace;
    m.trace_sink = &trace;
    m.load_program(kSched0, sizeof(kSched0) / 4);
    const uint64_t limit = 400;
    m.run_until(limit);

    // The program observed the latched Timer0 line (bit 4 only).
    EXPECT_EQ(m.read32(0x00000200u), ps1::sysdev::kIrqTimer0);
    // It acknowledged afterwards; one-shot MODE means the latch stays clear.
    EXPECT_EQ(m.read32(0x1F801070u), 0u);
    // Counter kept counting between the event and the sample.
    const uint32_t sampled = m.read32(0x00000204u);
    EXPECT_TRUE(sampled > 0 && sampled < 60);

    // Trace: two lines per executed instruction batch of 2 cycles...
    const size_t expect_lines =
        static_cast<size_t>(limit / ps1::sysdev::kCyclesPerInstruction);
    size_t lines = 0;
    for (char ch : trace)
        if (ch == '\n') ++lines;
    EXPECT_EQ(lines, expect_lines);
    // ...in the canonical shape.
    EXPECT_TRUE(trace.compare(0, 11, "pc=80010000") == 0);
    EXPECT_NE(trace.find("op=3C081F80"), std::string::npos);
    EXPECT_NE(trace.find("irq=0010"), std::string::npos);  // visible in poll
}

