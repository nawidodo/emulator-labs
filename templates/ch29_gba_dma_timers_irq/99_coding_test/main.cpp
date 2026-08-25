#define LABSTEST_MAIN
#include "labstest.hpp"
#include "capture.hpp"

using namespace gba;

TEST(capture, fires_exactly_on_lines_2_to_162) {
    EXPECT_FALSE(capture_fires_on_line(0));
    EXPECT_FALSE(capture_fires_on_line(1));
    EXPECT_TRUE(capture_fires_on_line(2));
    EXPECT_TRUE(capture_fires_on_line(3));
    EXPECT_TRUE(capture_fires_on_line(100));
    EXPECT_TRUE(capture_fires_on_line(161));
    EXPECT_TRUE(capture_fires_on_line(162));
    EXPECT_FALSE(capture_fires_on_line(163));
    EXPECT_FALSE(capture_fires_on_line(227));
}

TEST(capture, count_decode) {
    EXPECT_EQ(capture_units(4), 4u);
    EXPECT_EQ(capture_units(1), 1u);
    EXPECT_EQ(capture_units(0), 0x10000u);  // full range
}

TEST(capture, finishes_after_last_line) {
    EXPECT_FALSE(capture_finished_after(161));
    EXPECT_TRUE(capture_finished_after(162));
    EXPECT_TRUE(capture_finished_after(200));
}
