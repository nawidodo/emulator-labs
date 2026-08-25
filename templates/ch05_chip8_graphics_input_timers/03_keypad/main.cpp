#define LABSTEST_MAIN
#include "labstest.hpp"
#include "keypad.hpp"
#include <cstddef>

TEST(keypad, press_release_roundtrip) {
    chip8::Keypad kp;
    EXPECT_FALSE(kp.is_down(5));
    kp.press(5);
    EXPECT_TRUE(kp.is_down(5));
    kp.release(5);
    EXPECT_FALSE(kp.is_down(5));
}

TEST(keypad, keys_are_independent) {
    chip8::Keypad kp;
    kp.press(0);
    kp.press(15);
    EXPECT_TRUE(kp.is_down(0));
    EXPECT_FALSE(kp.is_down(1));
    EXPECT_TRUE(kp.is_down(15));
    kp.release(0);
    EXPECT_FALSE(kp.is_down(0));
    EXPECT_TRUE(kp.is_down(15));
}

TEST(keypad, first_down_empty_returns_minus_one) {
    chip8::Keypad kp;
    EXPECT_EQ(kp.first_down(), -1);
}

TEST(keypad, first_down_picks_lowest_key) {
    chip8::Keypad kp;
    kp.press(9);
    kp.press(2);
    kp.press(14);
    EXPECT_EQ(kp.first_down(), 2);
}

TEST(keypad, feed_parse_basic_lines) {
    const chip8::InputFeed feed =
        chip8::InputFeed::parse("5\n.\nA3\n\nF\n");
    EXPECT_EQ(feed.frames.size(), static_cast<std::size_t>(5));
    EXPECT_EQ(feed.frames[0], std::string("5"));
    EXPECT_EQ(feed.frames[1], std::string(""));   // '.' = empty frame
    EXPECT_EQ(feed.frames[2], std::string("A3"));
    EXPECT_EQ(feed.frames[3], std::string(""));   // blank line = empty frame
    EXPECT_EQ(feed.frames[4], std::string("F"));
}

TEST(keypad, feed_apply_sets_exact_state) {
    const chip8::InputFeed feed = chip8::InputFeed::parse("25F\n.3\n");
    chip8::Keypad kp;
    // Pre-hold a key that frame 0 does not list: apply must release it.
    kp.press(7);

    feed.apply(kp, 0);
    EXPECT_TRUE(kp.is_down(2));
    EXPECT_TRUE(kp.is_down(5));
    EXPECT_TRUE(kp.is_down(15));
    EXPECT_FALSE(kp.is_down(7));   // released: not listed this frame

    feed.apply(kp, 1);
    EXPECT_FALSE(kp.is_down(2));
    EXPECT_TRUE(kp.is_down(3));
}

TEST(keypad, feed_past_end_releases_everything) {
    const chip8::InputFeed feed = chip8::InputFeed::parse("9\n");
    chip8::Keypad kp;
    feed.apply(kp, 0);
    EXPECT_TRUE(kp.is_down(9));
    feed.apply(kp, 1);              // beyond the feed
    EXPECT_FALSE(kp.is_down(9));
}

TEST(keypad, wait_for_key_finds_first_press_frame) {
    const chip8::InputFeed feed =
        chip8::InputFeed::parse(".\n.\n7\n.\n2\n");
    int key = -99;
    EXPECT_EQ(chip8::wait_for_key(feed, 0, &key), 2);
    EXPECT_EQ(key, 7);
}

TEST(keypad, wait_for_key_uses_lowest_when_several_held) {
    const chip8::InputFeed feed = chip8::InputFeed::parse(".\nCB4\n");
    int key = -99;
    EXPECT_EQ(chip8::wait_for_key(feed, 0, &key), 1);
    EXPECT_EQ(key, 4);
}

TEST(keypad, wait_for_key_respects_start_frame) {
    const chip8::InputFeed feed = chip8::InputFeed::parse("6\n.\n8\n");
    int key = -99;
    // Waiting starts at frame 1: the frame-0 press never happened for us.
    EXPECT_EQ(chip8::wait_for_key(feed, 1, &key), 2);
    EXPECT_EQ(key, 8);
}

TEST(keypad, wait_for_key_feed_exhausted_returns_minus_one) {
    const chip8::InputFeed feed = chip8::InputFeed::parse(".\n.\n");
    int key = -99;
    EXPECT_EQ(chip8::wait_for_key(feed, 0, &key), -1);
}
