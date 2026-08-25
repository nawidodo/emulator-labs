#define LABSTEST_MAIN
#include "labstest.hpp"
#include "decode.hpp"

namespace {

constexpr uint8_t PAT32[] = {0x78, 0x56, 0x34, 0x12};
constexpr uint8_t DEAD[] = {0xEF, 0xBE, 0xAD, 0xDE};
// Every be16 buffer carries one spare byte so the seeded off-by-one stays
// in-bounds and fails as a VALUE mismatch instead of undefined behavior.
constexpr uint8_t PAT_BE[] = {0xBE, 0xEF, 0x00};
constexpr uint8_t PAT_BE2[] = {0x12, 0x34, 0x99};

}  // namespace

TEST(debug_le32, basic_word) {
    EXPECT_EQ(ch01_debug::read_le32(PAT32), 0x12345678u);
}

TEST(debug_le32, deadbeef) {
    EXPECT_EQ(ch01_debug::read_le32(DEAD), 0xDEADBEEFu);
}

TEST(debug_le32, msb_set) {
    const uint8_t buf[] = {0xFF, 0xFF, 0xFF, 0x80};
    EXPECT_EQ(ch01_debug::read_le32(buf), 0x80FFFFFFu);
}

TEST(debug_le32, consistent_with_le16_halves) {
    // The low half must match read_le16 at p, the high half at p+2.
    EXPECT_EQ(ch01_debug::read_le32(PAT32) & 0xFFFFu,
              ch01_debug::read_le16(PAT32));
    EXPECT_EQ(ch01_debug::read_le32(PAT32) >> 16,
              ch01_debug::read_le16(PAT32 + 2));
}

TEST(debug_be16, basic) {
    EXPECT_EQ(ch01_debug::read_be16(PAT_BE), 0xBEEFu);
}

TEST(debug_be16, high_byte_first) {
    EXPECT_EQ(ch01_debug::read_be16(PAT_BE2), 0x1234u);
}

TEST(debug_be16, ignores_trailing_spare_byte) {
    // A correct decoder never looks past the two field bytes.
    EXPECT_EQ(ch01_debug::read_be16(PAT_BE), 0xBEEFu);
    EXPECT_EQ(ch01_debug::read_be16(PAT_BE2), 0x1234u);
}

TEST(debug_context, le16_reference_is_correct) {
    const uint8_t buf[] = {0x34, 0x12};
    EXPECT_EQ(ch01_debug::read_le16(buf), 0x1234);
}
