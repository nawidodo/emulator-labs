#define LABSTEST_MAIN
#include "labstest.hpp"

#include <vector>

#include "coding.hpp"

namespace {

struct Fixture {
    std::vector<uint8_t> bank0 = std::vector<uint8_t>(0x10000, 0);
    std::vector<uint8_t> bank7e = std::vector<uint8_t>(0x10000, 0);
    snescpu::Mem mem;

    Fixture() {
        mem.banks[0x00] = bank0.data();
        mem.banks[0x7E] = bank7e.data();
    }
};

constexpr uint16_t kD = 0x0200;

}  // namespace

TEST(coding, pointer_indirection_and_db_bank) {
    Fixture f;
    // Pointer word at D+$10 -> $1234; data lives in the DB bank.
    f.bank0[kD + 0x10] = 0x34;
    f.bank0[kD + 0x11] = 0x12;
    f.bank7e[0x1236] = 0x9C;  // ptr $1234 + Y $0002
    f.bank7e[0x1237] = 0xAB;
    f.bank0[0x1236] = 0xFF;  // decoy: must NOT be read (bank zero)

    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~(snescpu::FM | snescpu::FX));  // 16-bit A and Y
    c.d = kD;
    c.db = 0x7E;
    c.y = 0x0002;

    bool crossed = true;  // must be OVERWRITTEN, not OR-ed
    const int cycles = snescpu::lda_dp_indirect_y(c, f.mem, 0x10, &crossed);
    EXPECT_EQ(cycles, 5);
    EXPECT_FALSE(crossed);
    EXPECT_EQ(c.a, 0xAB9C);  // from bank $7E, not from bank $00
}

TEST(coding, wide_y_page_cross_penalty) {
    Fixture f;
    f.bank0[kD + 0x00] = 0xFF;  // ptr low
    f.bank0[kD + 0x01] = 0x20;  // ptr high -> ptr=$20FF
    f.bank0[0x2101] = 0x11;     // ptr+Y with Y=2 -> $2101 low byte
    f.bank0[0x2102] = 0x40;     // high byte

    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~(snescpu::FM | snescpu::FX));
    c.d = kD;
    c.db = 0x00;
    c.y = 0x0002;

    bool crossed = false;
    const int cycles = snescpu::lda_dp_indirect_y(c, f.mem, 0x00, &crossed);
    EXPECT_TRUE(crossed);
    EXPECT_EQ(cycles, 5);       // base only; caller charges the cross
    EXPECT_EQ(c.a, 0x4011);
}

TEST(coding, narrow_a_reads_one_byte) {
    Fixture f;
    f.bank0[kD + 0x06] = 0x30;
    f.bank0[0x3010] = 0x7F;
    f.bank0[0x3011] = 0xEE;     // must NOT be read in 8-bit mode

    snescpu::Cpu c;
    c.a = 0xCD00;               // hidden B must survive
    c.e = false;
    c.p |= snescpu::FM;         // A 8-bit
    c.p &= uint8_t(~snescpu::FX);
    c.y = 0x0010;
    c.d = kD;

    bool crossed = false;
    snescpu::lda_dp_indirect_y(c, f.mem, 0x05, &crossed);
    EXPECT_FALSE(crossed);
    EXPECT_EQ(c.a, 0xCD7F);     // one byte loaded, B preserved
}

TEST(coding, eight_bit_y_can_still_cross) {
    Fixture f;
    f.bank0[kD + 0x02] = 0xF0;  // ptr = $21F0
    f.bank0[kD + 0x03] = 0x21;
    f.bank0[0x22DF] = 0x5A;     // ptr+Y with Y=$EF -> $22DF

    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~snescpu::FM);  // A 16-bit for an easy value check
    c.p |= snescpu::FX;            // Y is 8-bit now
    c.y = 0x00EF;
    c.d = kD;

    bool crossed = false;
    snescpu::lda_dp_indirect_y(c, f.mem, 0x02, &crossed);
    EXPECT_TRUE(crossed);
    EXPECT_EQ(c.a, 0x005A);
}
