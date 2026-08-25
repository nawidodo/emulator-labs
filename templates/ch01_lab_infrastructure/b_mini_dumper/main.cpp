#define LABSTEST_MAIN
#include "labstest.hpp"
#include "mini.hpp"

TEST(mini_b, pads_to_eight) {
    EXPECT_EQ(mini_b::offset8(0xA), std::string("0000000a"));
}

TEST(mini_b, lowercase_full_width) {
    EXPECT_EQ(mini_b::offset8(0xDEADBEEFu), std::string("deadbeef"));
}
