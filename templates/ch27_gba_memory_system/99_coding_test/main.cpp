#define LABSTEST_MAIN
#include "labstest.hpp"
#include "coding_bus.hpp"

using namespace coding;

TEST(flash, window_detection) {
    EXPECT_TRUE(flash_present(0x0F000000u));
    EXPECT_TRUE(flash_present(0x0F7FFFFEu));
    EXPECT_FALSE(flash_present(0x0E000000u));
    EXPECT_FALSE(flash_present(0x10000000u));
}

TEST(flash, mirror_folding) {
    EXPECT_EQ(flash_offset(0x0F000010u), 0x10u);
    EXPECT_EQ(flash_offset(0x0F00010010u & 0xFFFFFFFFu), 0x0010u);
    // +64 K mirrors onto the same page.
    EXPECT_EQ(flash_offset(0x0F00010010u), flash_offset(0x0F00000010u));
    EXPECT_EQ(flash_offset(0x0F7FFCDEu), 0xFCDEu);
}

TEST(flash, burst_totals_follow_datasheet) {
    EXPECT_EQ(burst_total(0), 0ull);
    EXPECT_EQ(burst_total(1), 8ull);              // N only
    EXPECT_EQ(burst_total(2), 11ull);             // N + S
    EXPECT_EQ(burst_total(8), 8ull + 7ull * 3);   // N + 7S
}

TEST(hidden, flash_hidden_long_burst) {
    // Unseen in public material: 64 halfwords.
    EXPECT_EQ(burst_total(64), 8ull + 63ull * 3);
}
