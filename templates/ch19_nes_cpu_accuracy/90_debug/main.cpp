#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    RecordingBus bus;
    Cpu cpu{.bus = &bus};

    void prog(std::initializer_list<uint8_t> bytes, uint16_t base = 0x0600) {
        cpu.load_program(base, bytes.begin(), bytes.size());
    }
};

// Handler at $0700: INC $30 then RTI — the service counter.
constexpr uint8_t kHandler[] = {0xE6, 0x30, 0x40};

void install_handler(Rig& r) {
    r.bus.mem[0xFFFA] = 0x00;
    r.bus.mem[0xFFFB] = 0x07;
    for (size_t i = 0; i < sizeof kHandler; ++i)
        r.bus.mem[0x0700 + i] = kHandler[i];
}

}  // namespace

// Regression tests for both seeded bugs. RED until fixed; keep them in
// bug-report.md.

TEST(regression, rmw_writes_old_value_before_new) {
    Rig r;
    r.bus.mem[0x40] = 0xAA;
    r.prog({0xE6, 0x40});              // INC $40
    step(r.cpu);
    EXPECT_EQ(r.bus.mem[0x40], 0xAB);
    int old_value_writes = 0, final_writes = 0;
    for (const auto& a : r.bus.log) {
        if (a.write && a.addr == 0x40 && a.value == 0xAA) ++old_value_writes;
        if (a.write && a.addr == 0x40 && a.value == 0xAB) ++final_writes;
    }
    EXPECT_EQ(old_value_writes, 1);    // the buggy build writes AA never
    EXPECT_EQ(final_writes, 1);
}

TEST(regression, inc_zero_page_bills_five_cycles) {
    Rig r;
    r.bus.mem[0x40] = 1;
    r.prog({0xE6, 0x40});
    step(r.cpu);
    EXPECT_EQ(r.cpu.cycles, 5u);       // buggy build runs one short
}

TEST(regression, nmi_latch_serviced_exactly_once_while_line_held) {
    Rig r;
    install_handler(r);
    // Endless NOP sled so the core stays live for the whole window.
    r.prog({0xEA, 0xEA, 0xEA, 0x4C, 0x00, 0x06});
    set_nmi_line(r.cpu, true);         // single quiet->high edge
    run(r.cpu, 20);                    // ...but the line stays HIGH
    EXPECT_EQ(r.bus.mem[0x30], 1);     // buggy build services every step
}

TEST(regression, nmi_rearms_only_on_a_fresh_edge) {
    Rig r;
    install_handler(r);
    r.prog({0xEA, 0xEA, 0xEA, 0x4C, 0x00, 0x06});
    set_nmi_line(r.cpu, true);
    run(r.cpu, 6);
    set_nmi_line(r.cpu, false);
    set_nmi_line(r.cpu, true);         // new edge
    run(r.cpu, 6);
    EXPECT_EQ(r.bus.mem[0x30], 2);
}
