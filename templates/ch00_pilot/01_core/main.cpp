#define LABSTEST_MAIN
#include "labstest.hpp"
#include "core.hpp"

TEST(pilot, endian) {
    const uint8_t buf[] = {0x34, 0x12};
    EXPECT_EQ(pilot::read_le16(buf), 0x1234);
}
