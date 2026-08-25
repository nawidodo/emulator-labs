// Exercise 13.01 tests -- divider counting, DIV read/write semantics.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "divider.hpp"

TEST(divider, counts_up_in_4_cycle_blocks) {
    gb::Divider d;
    d.step(4);
    EXPECT_EQ(d.counter, 4);
    d.step(12);
    EXPECT_EQ(d.counter, 16);
}

TEST(divider, high_byte_read_matches_counter) {
    gb::Divider d;
    d.counter = 0x12AB;
    EXPECT_EQ(d.div(), 0x12);
    d.counter = 0xFF01;  // not wrapped to the visible byte yet
    EXPECT_EQ(d.div(), 0xFF);
}

TEST(divider, tick_granularity_is_256_t_cycles_per_div_step) {
    gb::Divider d;
    // One block short of a full DIV increment: still $00
    d.step(252);
    EXPECT_EQ(d.counter, 252);
    d.step(4);
    EXPECT_EQ(d.div(), 0x01);   // first visible tick at exactly 256 cycles
    d.step(254 * 256);
    EXPECT_EQ(d.counter, 65280);
    EXPECT_EQ(d.div(), 0xFF);   // only the high byte is visible
}

TEST(divider, write_resets_whole_internal_counter) {
    gb::Divider d;
    d.step(4092);               // counter = $0FFC
    EXPECT_EQ(d.counter, 0x0FFC);
    d.write_div();
    EXPECT_EQ(d.counter, 0x0000);
    EXPECT_EQ(d.div(), 0x00);
    // Counting resumes from zero, no residue in the low byte either.
    d.step(8);
    EXPECT_EQ(d.counter, 8);
}
