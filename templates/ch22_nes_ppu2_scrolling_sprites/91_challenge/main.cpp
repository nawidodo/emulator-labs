#define LABSTEST_MAIN
#include "labstest.hpp"

#include <string>

#include "timing.hpp"

using namespace nes22timing;

// Reference points cross-checked against NESdev "PPU scrolling" dot tables.
TEST(nes22time, increment_x_fires_every_eight_dots) {
    PpuTiming p;
    p.l.t = 0x0000;
    p.l.v = p.l.t;
    // Advance from (0,0) to (0,8): one coarse-X step must have happened.
    run_to(p, 0, 8);
    EXPECT_EQ(p.l.v & 0x001F, 1);
}

TEST(nes22time, copy_x_at_257_reloads_from_t) {
    PpuTiming p;
    p.l.t = 0x0403;          // NT1 + coarse X 3
    p.l.v = 0x0005;          // drifted away during the line
    run_to(p, 0, 257);
    EXPECT_EQ(p.l.v & 0x041F, 0x0403);
}

TEST(nes22time, increment_y_at_dot_256) {
    PpuTiming p;
    p.l.v = 0x0000;          // fine Y 0
    p.l.t = p.l.v;
    run_to(p, 0, 257);
    EXPECT_EQ(p.l.v >> 12, 1);   // fine Y advanced once at dot 256
}

TEST(nes22time, prerender_copy_y_restores_vertical) {
    PpuTiming p;
    p.line = 261;
    p.dot = 279;
    p.l.t = uint16_t(0x1000 | (3 << 5));   // fine Y=1 coarse Y=3
    p.l.v = 0;                             // garbage vertical state
    run_to(p, 261, 304);
    EXPECT_EQ((p.l.v & 0x7BE0), uint16_t(0x1000 | (3 << 5)));
}

TEST(nes22time, snapshot_text_is_stable) {
    PpuTiming p;
    p.line = 30; p.dot = 100;
    p.l.v = 0x1234; p.l.x = 5; p.l.w = true;
    EXPECT_EQ(snapshot_text(p), "line=30 dot=100 v=1234 x=5 w=1");
}
