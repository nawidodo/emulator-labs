#define LABSTEST_MAIN
#include "labstest.hpp"

#include "shift_register.hpp"

// Exhaustive contract tests for the shift register. The software model in
// the tests is written INDEPENDENTLY (plain bit loop) so it can catch an
// off-by-one in the implementation rather than mirror it.

namespace {

// Independent reference: window of 8 bits starting at `amt`, taken from a
// linear 16-bit value (bit 0 = LSB).
uint8_t ref_window(uint16_t v, unsigned amt) {
    amt &= 7;
    uint8_t out = 0;
    for (unsigned i = 0; i < 8; ++i)
        if ((v >> (amt + i)) & 1) out |= uint8_t(1u << i);
    return out;
}

}  // namespace

TEST(shift, two_writes_fill_low_then_high) {
    ShiftRegister sr;
    sr.write_data(0xDE);          // first write lands in the LOW byte...
    EXPECT_EQ(sr.raw(), 0xDE00);  // ...via the high-half insertion
    sr.write_data(0xAD);          // second write pushes DE down to low
    EXPECT_EQ(sr.raw(), 0xADDE);
}

TEST(shift, amount_wraps_modulo_eight) {
    ShiftRegister sr;
    sr.set_amount(8);
    EXPECT_EQ(sr.amount(), 0);
    sr.set_amount(9);
    EXPECT_EQ(sr.amount(), 1);
    sr.set_amount(15);
    EXPECT_EQ(sr.amount(), 7);
    sr.set_amount(255);
    EXPECT_EQ(sr.amount(), 7);
}

TEST(shift, reads_match_reference_model_exhaustively) {
    // All 65536 register fillings x all 8 amounts against the independent
    // bit-loop model. An off-by-one anywhere diverges here.
    for (uint32_t lo = 0; lo < 256; ++lo) {
        for (uint32_t hi = 0; hi < 256; ++hi) {
            ShiftRegister sr;
            sr.write_data(uint8_t(lo));
            sr.write_data(uint8_t(hi));
            // After the hardware shift dance: high=hi, low=lo.
            const uint16_t expect_raw =
                uint16_t((hi << 8) | lo);
            if (sr.raw() != expect_raw) {
                EXPECT_EQ(sr.raw(), expect_raw);
                return;
            }
            for (unsigned amt = 0; amt < 8; ++amt) {
                sr.set_amount(uint8_t(amt));
                const uint8_t got = sr.read();
                const uint8_t want = ref_window(expect_raw, amt);
                if (got != want) {
                    EXPECT_EQ(got, want);
                    return;
                }
            }
        }
    }
}

TEST(shift, known_value_end_to_end) {
    // The canonical example used across the chapter fixtures:
    //   write 0xDE, write 0xAD, amount 3 -> IN 3 reads 0xBB.
    ShiftRegister sr;
    sr.write_data(0xDE);
    sr.write_data(0xAD);
    sr.set_amount(3);
    EXPECT_EQ(sr.read(), 0xBB);
}

TEST(shift, amount_zero_reads_the_low_byte) {
    ShiftRegister sr;
    sr.write_data(0x11);
    sr.write_data(0x22);          // raw = 0x2211
    sr.set_amount(0);
    EXPECT_EQ(sr.read(), 0x11);   // zero shift exposes the low half
    sr.set_amount(8);             // wraps to 0 — same answer
    EXPECT_EQ(sr.read(), 0x11);
}

TEST(shift, amount_seven_exposes_only_one_new_bit) {
    ShiftRegister sr;
    sr.write_data(0xFF);
    sr.write_data(0x00);          // raw = 0x00FF
    sr.set_amount(7);
    EXPECT_EQ(sr.read(), 0x01);   // only bit 7 of LOW survives the slide
}

TEST(shift, repeated_writes_shift_history_through) {
    ShiftRegister sr;
    const uint8_t stream[] = {0x01, 0x02, 0x03, 0x04};
    for (uint8_t b : stream) sr.write_data(b);
    EXPECT_EQ(sr.raw(), 0x0403);  // last two writes fill high,low
}
