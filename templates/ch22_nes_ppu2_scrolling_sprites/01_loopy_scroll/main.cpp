#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>

#include "loopy.hpp"

using namespace nes22scroll;

// Reference sequences cross-checked against NESdev "PPU scrolling" and
// the nestest.log-style worked examples.

TEST(nes22scroll, ctrl_write_selects_nametable_in_t) {
    Loopy l;
    l.t = 0x0000;
    ctrl_write(l, 0x03);
    EXPECT_EQ(l.t, 0x0C00);
    l.t = 0x7FFF;
    ctrl_write(l, 0x02);
    // Bits 10-11 replaced by data bits 0-1; everything else preserved.
    EXPECT_EQ(l.t, 0x7BFF);
}

TEST(nes22scroll, scroll_first_write_latches_fine_x_and_coarse_x) {
    Loopy l;
    l.t = 0x7FFF;
    scroll_write(l, 0xF5);            // fine X = 5, coarse X = 30
    EXPECT_EQ(l.x, 5);
    EXPECT_EQ(l.t & 0x001F, 0x1E);    // coarse X = 30
    EXPECT_EQ(l.w, true);
}

TEST(nes22scroll, scroll_second_write_sets_fine_y_coarse_y_clears_w) {
    Loopy l;
    l.t = 0x0000;
    scroll_write(l, 0x9E);   // first: x=6 coarse X=19
    EXPECT_EQ(l.x, 6);
    scroll_write(l, 0xD7);   // second: fine Y=7 coarse Y=26
    EXPECT_EQ(l.t, uint16_t(0x7000 | (26 << 5) | 19));  // keeps coarse X
    EXPECT_EQ(l.w, false);
}
TEST(nes22scroll, addr_sequence_2088) {
    // Classic worked example: $2006 <- $20 then $88 gives v = $2088.
    Loopy l;
    addr_write(l, 0x20);
    EXPECT_EQ(l.v, 0);       // not yet copied
    addr_write(l, 0x88);
    EXPECT_EQ(l.v, 0x2088);
    EXPECT_EQ(l.t, 0x2088);
    EXPECT_EQ(l.w, false);
}

TEST(nes22scroll, addr_high_write_forces_bit14_clear) {
    Loopy l;
    l.t = 0x4000;
    addr_write(l, 0x3F);     // high half of palette address
    addr_write(l, 0x10);
    EXPECT_EQ(l.v, 0x3F10);  // bit 14 dropped: t was masked with 0x00FF first
}

TEST(nes22scroll, status_read_resets_toggle) {
    Loopy l;
    scroll_write(l, 0x11);
    EXPECT_EQ(l.w, true);
    status_read(l);
    EXPECT_EQ(l.w, false);
    // Next $2005 write is a FIRST write again.
    scroll_write(l, 0x07);
    EXPECT_EQ(l.x, 7);
}

TEST(nes22scroll, data_access_increment_by_ctrl_bit2) {
    Loopy l;
    l.v = 0x0105;
    data_access(l, 0x00);
    EXPECT_EQ(l.v, 0x0106);
    data_access(l, 0x04);
    EXPECT_EQ(l.v, 0x0126);   // +32: next tile row
}

TEST(nes22scroll, data_access_wraps_at_15_bits) {
    Loopy l;
    l.v = 0x7FFF;
    data_access(l, 0x00);
    EXPECT_EQ(l.v, 0);        // bit 15 never set
    l.v = 0x7FE1;
    data_access(l, 0x04);
    EXPECT_EQ(l.v, 0x0001);
}

TEST(nes22scroll, increment_x_wraps_and_flips_horizontal_nametable) {
    Loopy l;
    l.v = 0x001F;             // coarse X = 31, NT0
    increment_x(l);
    EXPECT_EQ(l.v, 0x0400);   // coarse X = 0, NT1
    increment_x(l);
    EXPECT_EQ(l.v, 0x0401);
}

TEST(nes22scroll, increment_y_fine_y_first_then_wrap) {
    Loopy l;
    l.v = 0x0000;
    increment_y(l);
    EXPECT_EQ(l.v, 0x1000);   // fine Y 0 -> 1
    l.v = 0x7000 | (29 << 5);
    increment_y(l);
    EXPECT_EQ(l.v, 0x0800);   // coarse Y 29 -> 0, flip vertical NT bit
    l.v = 0x7000 | (31 << 5);
    increment_y(l);
    EXPECT_EQ(l.v, 0x0000);   // attribute row: no NT flip
}

TEST(nes22scroll, copy_x_moves_only_x_bits) {
    Loopy l;
    l.v = 0x7FFF;
    l.t = 0x0000;
    copy_x(l);
    EXPECT_EQ(l.v, 0x7BE0);   // bits 0-4 and 10 cleared in v
    l.t = 0x0023;
    copy_x(l);
    EXPECT_EQ(l.v, 0x7BE3);   // coarse X restored, NT bit still from t
}

TEST(nes22scroll, copy_y_moves_y_bits) {
    Loopy l;
    l.v = 0x0000;
    l.t = 0x73E0;             // fine Y=7, coarse Y=31, both NT bits
    copy_y(l);
    EXPECT_EQ(l.v, 0x73E0);
}
