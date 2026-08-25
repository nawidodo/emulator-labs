#define LABSTEST_MAIN
#include "labstest.hpp"
#include "display.hpp"

#include <algorithm>
#include <iterator>

TEST(display, starts_all_off) {
    chip8::Display d;
    for (int y = 0; y < chip8::kHeight; ++y)
        for (int x = 0; x < chip8::kWidth; ++x)
            EXPECT_FALSE(d.get(x, y));
}

TEST(display, set_get_roundtrip) {
    chip8::Display d;
    d.set(0, 0, true);
    d.set(63, 31, true);
    d.set(10, 5, true);
    EXPECT_TRUE(d.get(0, 0));
    EXPECT_TRUE(d.get(63, 31));
    EXPECT_TRUE(d.get(10, 5));
    EXPECT_FALSE(d.get(11, 5));   // neighbour untouched
    EXPECT_FALSE(d.get(10, 6));   // neighbour untouched
}

TEST(display, set_can_turn_pixel_back_off) {
    chip8::Display d;
    d.set(4, 4, true);
    EXPECT_TRUE(d.get(4, 4));
    d.set(4, 4, false);
    EXPECT_FALSE(d.get(4, 4));
}

TEST(display, get_out_of_bounds_is_false) {
    chip8::Display d;
    d.set(63, 31, true);
    EXPECT_FALSE(d.get(-1, 0));
    EXPECT_FALSE(d.get(0, -1));
    EXPECT_FALSE(d.get(64, 0));
    EXPECT_FALSE(d.get(63, 32));
}

TEST(display, set_out_of_bounds_ignored) {
    chip8::Display d;
    // Must not crash and must not corrupt neighbouring storage.
    d.set(-1, 0, true);
    d.set(64, 0, true);
    d.set(0, -1, true);
    d.set(0, 32, true);
    for (int y = 0; y < chip8::kHeight; ++y)
        for (int x = 0; x < chip8::kWidth; ++x)
            EXPECT_FALSE(d.get(x, y));
}

TEST(display, clear_resets_everything) {
    chip8::Display d;
    d.set(0, 0, true);
    d.set(63, 31, true);
    d.set(31, 15, true);
    d.clear();
    int lit = 0;
    for (int y = 0; y < chip8::kHeight; ++y)
        for (int x = 0; x < chip8::kWidth; ++x)
            if (d.get(x, y)) ++lit;
    EXPECT_EQ(lit, 0);
}

TEST(display, index_math_is_row_major) {
    // pixels[y * kWidth + x]: setting (3, 2) must not touch (2, 3).
    chip8::Display d;
    d.set(3, 2, true);
    EXPECT_TRUE(d.get(3, 2));
    EXPECT_FALSE(d.get(2, 3));
    EXPECT_EQ(chip8::kWidth * chip8::kHeight, 2048);
}
