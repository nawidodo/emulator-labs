// Tests for 90_debug: each suite targets exactly one seeded timer defect.
// All three must be fixed before the suite goes green.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "timer_debug.hpp"

TEST(debug_gate, enable_is_tac_bit2_not_bit0) {
    // Classic 4096 Hz setup: TAC=$04 -- enabled, select 00 (DIV bit 9,
    // one fall per 1024 T-cycles). The buggy gate thinks this is OFF.
    gbdbg::GateTimer t;
    t.tac = 0x04;
    t.step(1024 * 10);
    EXPECT_EQ(t.tima, 10);

    // TAC=$01: select bits say 01 but bit 2 is CLEAR -- nothing may tick.
    gbdbg::GateTimer off;
    off.tac = 0x01;
    off.step(160 * 20);
    EXPECT_EQ(off.tima, 0);
}

TEST(debug_disable, switching_off_with_tapped_bit_low_ticks_once) {
    gbdbg::DisableTimer t;
    t.cnt = 0x0000;   // select-01 tapped bit (DIV bit 3) reads 0
    t.tac = 0x05;     // enabled, select 01
    t.tima = 0x40;

    t.write_tac(0x01);  // disable, same select; tapped bit still 0
    EXPECT_EQ(t.tac, 0x01);
    EXPECT_EQ(t.tima, 0x41);  // exactly ONE increment from the disable edge

    // Control: disabling while the tapped bit reads 1 must NOT tick.
    gbdbg::DisableTimer high;
    high.cnt = 0x0008;  // DIV bit 3 set
    high.tac = 0x05;
    high.tima = 0x40;
    high.write_tac(0x01);
    EXPECT_EQ(high.tima, 0x40);
}

TEST(debug_reload, overflow_reloads_tma_exactly_once) {
    gbdbg::ReloadTimer t;
    t.tac = 0x05;      // enabled, select 01: one fall per 16 T-cycles
    t.tima = 0xFF;
    t.tma = 0x37;
    t.cnt = 0x0018;    // tapped bit set, falls on the second block

    t.step(16);        // exactly one fall inside
    EXPECT_TRUE(t.irq_strobe);   // interrupt line raised...
    EXPECT_EQ(t.tima, 0x37);     // ...and TIMA reloaded from TMA, not $00
}
