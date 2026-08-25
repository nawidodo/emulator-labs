#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "dbg_scroll.hpp"

using namespace nes22dbg;

// Regression: second $2005 write packs fine Y into t bits 12-14.
// RED while the defect is present.
TEST(nes22dbg, scroll_second_write_packs_fine_y) {
    Loopy l;
    l.t = 0x0000;
    scroll_write(l, 0x40);   // first: fine X 0, coarse X 8
    EXPECT_EQ(l.x, 0);
    EXPECT_EQ(l.w, true);
    scroll_write(l, 0xD7);   // second: fine Y = 7 (0xD7 & 7), coarse Y = 26
    EXPECT_EQ(l.t, uint16_t(0x7000 | (26 << 5) | 8));
}

TEST(nes22dbg, scroll_latch_round_trip) {
    Loopy l;
    scroll_write(l, 0x2B);   // x=3, coarse X=5
    scroll_write(l, 0x5A);   // fine Y=2, coarse Y=11
    EXPECT_EQ(l.x, 3);
    addr_write(l, uint8_t((l.t >> 8) & 0xFF));
    addr_write(l, uint8_t(l.t & 0xFF));
    // $2006 round-trip must preserve the scrolled state exactly.
    EXPECT_EQ(l.v, l.t);
    EXPECT_EQ(l.t >> 12, 2);
    EXPECT_EQ((l.t >> 5) & 0x1F, 11);
}

TEST(nes22dbg, status_read_reopens_first_half) {
    Loopy l;
    scroll_write(l, 0x08);
    EXPECT_EQ(l.w, true);
    status_read(l);
    scroll_write(l, 0xF0);   // now a FIRST write again: x=0, coarse X=30
    EXPECT_EQ(l.x, 0);
    EXPECT_EQ((l.t & 0x1F), 30);
}
