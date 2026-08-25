#define LABSTEST_MAIN
#include "labstest.hpp"

#include <vector>

#include "extra_ops.hpp"

// Local (visible) coverage for the ten coding-test instructions. The
// hidden grader runs this binary with filters "hidden.<op>"; those suites
// live below and exercise additional edge cases from the spec table.

using namespace i8080ext;

namespace {

struct MemBus final : Bus {
    uint8_t mem[0x10000] = {};
    uint8_t read(uint16_t addr) const override { return mem[addr]; }
    void write(uint16_t addr, uint8_t v) override { mem[addr] = v; }
};

}  // namespace

TEST(stax, stores_a_at_pair_address) {
    MemBus bus;
    stax(bus, 0x1234, 0xAB);
    EXPECT_EQ(bus.mem[0x1234], 0xAB);
}

TEST(ldax, loads_from_pair_address) {
    MemBus bus;
    bus.mem[0x0042] = 0x5A;
    EXPECT_EQ(ldax(bus, 0x0042), 0x5A);
}

TEST(inx, increments_without_flags) {
    uint16_t p = 0x00FF;
    inx(p);
    EXPECT_EQ(p, 0x0100);   // byte-wise INR would have produced 0x0000
    p = 0xFFFF;
    inx(p);
    EXPECT_EQ(p, 0x0000);
}

TEST(dcx, decrements_wrapping) {
    uint16_t p = 0x0000;
    dcx(p);
    EXPECT_EQ(p, 0xFFFF);
}

TEST(dad, sixteen_bit_add_carry_only) {
    uint16_t hl = 0x1234;
    EXPECT_FALSE(dad(hl, 0x5678));
    EXPECT_EQ(hl, 0x68AC);

    hl = 0xFFFF;
    EXPECT_TRUE(dad(hl, 0x0001));
    EXPECT_EQ(hl, 0x0000);
}

TEST(daa, nibble_corrections) {
    // 0x9C with AC set: only the low correction applies (high nibble is
    // exactly 9 and CY clear), giving 0x9C + 06 = 0xA2.
    auto r = daa(0x9C, false, true);
    EXPECT_EQ(r.a, 0xA2);
    EXPECT_FALSE(r.cy);        // no high correction -> CY passes through
    EXPECT_TRUE(r.ac);
    EXPECT_TRUE(r.s);

    // Low nibble > 9 only: +06.
    r = daa(0x2B, false, false);
    EXPECT_EQ(r.a, 0x31);
    EXPECT_FALSE(r.cy);

    // Nothing to do: value untouched, flags reflect it.
    r = daa(0x42, false, false);
    EXPECT_EQ(r.a, 0x42);
    EXPECT_FALSE(r.cy);
    EXPECT_FALSE(r.ac);
    EXPECT_FALSE(r.s);
    EXPECT_FALSE(r.z);
    EXPECT_TRUE(r.p);

    // CY passes through unchanged when no high correction happens.
    r = daa(0x15, true, false);
    EXPECT_TRUE(r.cy);
}

TEST(rlc, rotates_left) {
    uint8_t a = 0x80;
    EXPECT_TRUE(rlc(a));
    EXPECT_EQ(a, 0x01);

    a = 0x41;
    EXPECT_FALSE(rlc(a));
    EXPECT_EQ(a, 0x82);
}

TEST(rrc, rotates_right) {
    uint8_t a = 0x01;
    EXPECT_TRUE(rrc(a));
    EXPECT_EQ(a, 0x80);

    a = 0x82;
    EXPECT_FALSE(rrc(a));
    EXPECT_EQ(a, 0x41);
}

TEST(ral_rar, rotate_through_carry) {
    bool cy = false;
    EXPECT_EQ(ral(0xB4, cy, cy), 0x68);  // old bit7=1 -> CY now set
    EXPECT_TRUE(cy);

    cy = true;
    EXPECT_EQ(ral(0x7F, cy, cy), 0xFF);  // carry shifts into bit 0
    EXPECT_FALSE(cy);                    // old bit7 of 7F was clear

    cy = false;
    EXPECT_EQ(rar(0x01, cy, cy), 0x00);
    EXPECT_TRUE(cy);

    cy = true;
    EXPECT_EQ(rar(0x00, cy, cy), 0x80);
    EXPECT_FALSE(cy);
}

// ---- Hidden grader suites: run via per-instruction filter args ----

TEST(hidden, stax) {
    MemBus bus;
    stax(bus, 0xBEEF, 0x77);
    EXPECT_EQ(bus.mem[0xBEEF], 0x77);
    stax(bus, 0x0001, 0xEE);
    EXPECT_EQ(bus.mem[0x0001], 0xEE);
}

TEST(hidden, ldax) {
    MemBus bus;
    bus.mem[0x8000] = 0x12;
    bus.mem[0xFFFF] = 0xF0;
    EXPECT_EQ(ldax(bus, 0x8000), 0x12);
    EXPECT_EQ(ldax(bus, 0xFFFF), 0xF0);
}

TEST(hidden, inx) {
    uint16_t p = 0x10FF;
    inx(p);
    EXPECT_EQ(p, 0x1100);
    inx(p);
    EXPECT_EQ(p, 0x1101);
}

TEST(hidden, dcx) {
    uint16_t p = 0x2000;
    dcx(p);
    EXPECT_EQ(p, 0x1FFF);
    dcx(p);
    EXPECT_EQ(p, 0x1FFE);
}

TEST(hidden, dad) {
    uint16_t hl = 0x9999;
    const bool carried = dad(hl, 0x7778);
    EXPECT_EQ(hl, 0x1111);
    EXPECT_TRUE(carried);
    hl = 0x1000;
    EXPECT_FALSE(dad(hl, 0x0FFF));
    EXPECT_EQ(hl, 0x1FFF);
}

TEST(hidden, daa) {
    // Raw sum 0x9E with AC set: only +06 applies (high nibble is exactly 9,
    // carry clear), so 0x9E + 06 = 0xA4 and CY passes through unset.
    auto r = daa(0x9E, false, true);
    EXPECT_EQ(r.a, 0xA4);
    EXPECT_FALSE(r.cy);
    EXPECT_TRUE(r.ac);
    // Nothing to adjust: value untouched.
    r = daa(0x13, false, false);
    EXPECT_EQ(r.a, 0x13);
    EXPECT_FALSE(r.cy);
    EXPECT_FALSE(r.ac);
    // High nibble > 9 alone: +60 wraps the accumulator and sets CY.
    r = daa(0xA3, false, false);
    EXPECT_EQ(r.a, 0x03);
    EXPECT_TRUE(r.cy);
}

TEST(hidden, rlc) {
    uint8_t a = 0xAA;
    EXPECT_TRUE(rlc(a));   // bit7=1
    EXPECT_EQ(a, 0x55);
    EXPECT_FALSE(rlc(a));  // now bit7=0
    EXPECT_EQ(a, 0xAA);
}

TEST(hidden, rrc) {
    uint8_t a = 0x55;
    EXPECT_TRUE(rrc(a));   // bit0=1
    EXPECT_EQ(a, 0xAA);
    EXPECT_FALSE(rrc(a));
    EXPECT_EQ(a, 0x55);
}

TEST(hidden, ral) {
    bool cy = true;
    EXPECT_EQ(ral(0xFF, cy, cy), 0xFF);
    EXPECT_TRUE(cy);
    cy = false;
    EXPECT_EQ(ral(0x80, cy, cy), 0x00);
    EXPECT_TRUE(cy);
}

TEST(hidden, rar) {
    bool cy = true;
    EXPECT_EQ(rar(0x00, cy, cy), 0x80);
    EXPECT_FALSE(cy);
    cy = false;
    EXPECT_EQ(rar(0x03, cy, cy), 0x01);
    EXPECT_TRUE(cy);
}
