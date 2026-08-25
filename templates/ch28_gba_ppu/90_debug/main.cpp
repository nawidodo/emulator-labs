#define LABSTEST_MAIN
#include "labstest.hpp"
#include "debug_scene.hpp"

using namespace gba;

// These expectations encode REAL GBA behavior; the seeded bug fails them.
TEST(priority, equal_prio_sprite_wins) {
    EXPECT_EQ(resolve_top(true, 1, true, 1), Winner::kSprite);
    EXPECT_EQ(resolve_top(true, 0, true, 0), Winner::kSprite);
}

TEST(priority, lower_value_wins) {
    EXPECT_EQ(resolve_top(true, 0, true, 1), Winner::kBackground);
    EXPECT_EQ(resolve_top(true, 1, true, 2), Winner::kBackground);
    EXPECT_EQ(resolve_top(true, 3, true, 1), Winner::kSprite);
}

TEST(priority, transparent_never_occludes) {
    EXPECT_EQ(resolve_top(true, 3, false, 0), Winner::kBackground);
    EXPECT_EQ(resolve_top(false, 0, false, 0), Winner::kBackdrop);
}
