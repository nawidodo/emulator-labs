#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "dbg_mmc3.hpp"

using nes23dbg::Mmc3;
using nes23map::Cart;

namespace {

Cart irq_cart() {
    Cart c;
    c.prg.assign(8 * 8192, 0xEA);
    return c;
}

}  // namespace

// Sanity schedule: with no mid-count rewrite, latch L gives an IRQ every
// L+1 edges. Passes before AND after the fix.
TEST(nes23dbg, basic_period_is_latch_plus_one) {
    Mmc3 m(irq_cart());
    m.cpu_write(0xC000, 3);
    m.cpu_write(0xE001, 0);
    m.cpu_write(0xC001, 0);
    for (int i = 0; i < 4; ++i) {
        m.a12_edge();
        EXPECT_FALSE(m.irq_line());
    }
    m.a12_edge();
    EXPECT_TRUE(m.irq_line());
}

// Regression: the golden interrupt log for a mid-count period rewrite.
// The counter must FINISH the old countdown; the new latch lands only at
// the next reload. RED while the defect is present.
TEST(nes23dbg, mid_count_rewrite_keeps_running_the_old_period) {
    Mmc3 m(irq_cart());
    m.cpu_write(0xE001, 0);   // enable
    m.cpu_write(0xC000, 5);
    m.cpu_write(0xC001, 0);   // reload on next edge
    m.a12_edge();             // counter = 5
    m.a12_edge();             // counter = 4
    m.cpu_write(0xC000, 1);   // game shortens the period mid-frame...
    // Expected: 4 -> 3 -> 2 -> 1 -> 0, then reload to 1 + assert on the
    // edge AFTER hitting zero.
    for (int expect : {3, 2, 1, 0}) {
        m.a12_edge();
        if (m.irq_line()) EXPECT_EQ(m.counter(), -9999);  // IRQ too early!
        EXPECT_EQ(m.counter(), expect);
    }
    EXPECT_FALSE(m.irq_line());      // not yet — old period still running
    m.a12_edge();                    // reload from (new) latch = 1
    EXPECT_TRUE(m.irq_line());
    EXPECT_EQ(m.counter(), 1);
}
