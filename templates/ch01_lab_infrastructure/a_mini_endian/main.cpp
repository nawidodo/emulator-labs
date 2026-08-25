#define LABSTEST_MAIN
#include "labstest.hpp"
#include "mini.hpp"

TEST(mini_a, le16) {
    const uint8_t buf[] = {0x34, 0x12};
    EXPECT_EQ(mini_a::read_le16(buf), 0x1234);
}

TEST(mini_a, le16_swapped_bytes_differ) {
    const uint8_t buf[] = {0x01, 0x80};
    EXPECT_EQ(mini_a::read_le16(buf), 0x8001);
}
