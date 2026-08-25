#define LABSTEST_MAIN
#include "labstest.hpp"
#include "endian.hpp"

// Fixture bytes chosen so every byte position carries a distinct value;
// any permutation error in an assembly function shows up as a mismatch.
namespace {

constexpr uint8_t PAT16[] = {0x34, 0x12};
constexpr uint8_t PAT16_BE[] = {0xBE, 0xEF};
constexpr uint8_t PAT32[] = {0x78, 0x56, 0x34, 0x12};

}  // namespace

TEST(endian, le16_basic) {
    EXPECT_EQ(ch01::read_le16(PAT16), 0x1234);
}

TEST(endian, le16_low_byte_first) {
    // Same bytes interpreted as big-endian give a different answer; this
    // pins the ORDER, not just the value.
    EXPECT_NE(ch01::read_le16(PAT16), ch01::read_be16(PAT16));
}

TEST(endian, le16_high_byte_set) {
    const uint8_t buf[] = {0xFF, 0x80};
    EXPECT_EQ(ch01::read_le16(buf), 0x80FF);
}

TEST(endian, le16_zero_roundtrip) {
    const uint8_t buf[] = {0x00, 0x00};
    EXPECT_EQ(ch01::read_le16(buf), 0);
}

TEST(endian, be16_basic) {
    EXPECT_EQ(ch01::read_be16(PAT16_BE), 0xBEEF);
}

TEST(endian, be16_matches_manual_shift) {
    const uint8_t buf[] = {0xAB, 0xCD};
    uint16_t manual = uint16_t(uint16_t(buf[0]) << 8 | buf[1]);
    EXPECT_EQ(ch01::read_be16(buf), manual);
}

TEST(endian, le32_basic) {
    EXPECT_EQ(ch01::read_le32(PAT32), 0x12345678);
}

TEST(endian, le32_deadbeef) {
    const uint8_t buf[] = {0xEF, 0xBE, 0xAD, 0xDE};
    EXPECT_EQ(ch01::read_le32(buf), 0xDEADBEEF);
}

TEST(endian, le32_msb_set) {
    const uint8_t buf[] = {0xFF, 0xFF, 0xFF, 0x80};
    EXPECT_EQ(ch01::read_le32(buf), 0x80FFFFFFu);
}

TEST(bits, low_nibble) {
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 0, 4), 0xFu);
}

TEST(bits, second_nibble) {
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 4, 4), 0xEu);
}

TEST(bits, byte_extract_each_lane) {
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 0, 8), 0xEFu);
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 8, 8), 0xBEu);
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 16, 8), 0xADu);
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 24, 8), 0xDEu);
}

TEST(bits, single_bit_top) {
    EXPECT_EQ(ch01::bits(0x80000000u, 31, 1), 1u);
}

TEST(bits, single_bit_zero_everywhere_else) {
    EXPECT_EQ(ch01::bits(0x00000001u, 0, 1), 1u);
    EXPECT_EQ(ch01::bits(0x00000001u, 1, 1), 0u);
}

TEST(bits, full_width) {
    EXPECT_EQ(ch01::bits(0xDEADBEEFu, 0, 32), 0xDEADBEEFu);
}

TEST(bits, masked_window_all_ones) {
    EXPECT_EQ(ch01::bits(0xFFFFFFFFu, 8, 4), 0xFu);
}

TEST(bits, masked_window_mixed) {
    // 0xF0F0F0F0 >> 4 = 0x0F0F0F0F, low byte = 0x0F
    EXPECT_EQ(ch01::bits(0xF0F0F0F0u, 4, 8), 0x0Fu);
}
