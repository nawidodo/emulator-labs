#define LABSTEST_MAIN
#include "labstest.hpp"
#include "mini.hpp"

TEST(mini_c, nibble_and_full_width) {
    EXPECT_EQ(mini_c::bits(0xDEADBEEFu, 8, 8), 0xBEu);
    EXPECT_EQ(mini_c::bits(0xDEADBEEFu, 0, 32), 0xDEADBEEFu);
}

TEST(mini_c, single_top_bit) {
    EXPECT_EQ(mini_c::bits(0x80000000u, 31, 1), 1u);
}
