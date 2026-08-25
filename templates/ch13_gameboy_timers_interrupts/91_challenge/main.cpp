// Exercise 13.91 tests -- the challenge end-to-end: the assembled probe
// program (embedded byte-for-byte, see fixtures.hpp) driven through the
// same machine the runner uses, plus exact log-format pins.
#define LABSTEST_MAIN
#include <cstdint>
#include <string>

#include "labstest.hpp"
#include "../04_interrupt_delivery/machine.hpp"
#include "fixtures.hpp"
#include "format.hpp"

TEST(challenge, probe_counts_interrupts_in_hram) {
    gb::TimerMachine m;
    m.load(ch13_fixtures::timer_probe);

    m.run(200000);  // 6 overflows at 32768-cycle spacing fit inside

    EXPECT_EQ(m.ram.mem[0xFF80], 6);
    EXPECT_TRUE(m.cpu.halted);
    EXPECT_TRUE(m.ctl.ime);  // last RETI re-enabled interrupts
}

TEST(challenge, probe_log_has_one_line_pair_per_event) {
    gb::TimerMachine m;
    m.load(ch13_fixtures::timer_probe);

    std::string log;
    long events = 0;
    while (!m.cpu.trap && m.cyc < 200000) {
        if (m.cpu.halted && (m.ctl.flags & m.ctl.enabled) != 0)
            m.cpu.halted = false;
        const gb::StepReport rep = m.step_once();
        if (rep.overflow) {
            log += gbfmt::overflow_line(m.cyc);
            ++events;
        }
        if (rep.serviced) log += gbfmt::irq_line(m.cyc, rep.vector, true);
    }
    log += gbfmt::state_line(m);

    // Every overflow is serviced: equal counts, alternating lines.
    size_t overflows = 0;
    for (size_t pos = log.find("tima_overflow");
         pos != std::string::npos;
         pos = log.find("tima_overflow", pos + 1))
        ++overflows;
    EXPECT_EQ(overflows, static_cast<size_t>(events));
    EXPECT_EQ(events, 6L);
}

TEST(format, overflow_line_is_exact) {
    EXPECT_EQ(gbfmt::overflow_line(32768), "cyc=32768 tima_overflow\n");
}

TEST(format, irq_line_is_exact) {
    EXPECT_EQ(gbfmt::irq_line(32808, 0x50, true),
              "cyc=32808 irq vector=50 ime=1\n");
    EXPECT_EQ(gbfmt::irq_line(100, 0x40, false),
              "cyc=100 irq vector=40 ime=0\n");
}

TEST(format, state_line_is_exact) {
    gb::TimerMachine m;
    m.load(ch13_fixtures::timer_probe);
    const std::string line = gbfmt::state_line(m);
    // Keys lowercase, hex uppercase, whitespace-separated key=value tokens.
    EXPECT_EQ(line.substr(0, 9), "state af=");
    EXPECT_NE(line.find(" tima=00"), std::string::npos);  // fresh device
    EXPECT_NE(line.find(" tac=F8"), std::string::npos);   // reads OR $F8
}
