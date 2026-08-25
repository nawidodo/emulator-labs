// Tests for 03_echo_alias: the E000-FDFF window must behave as the
// same silicon as C000-DDFF, byte for byte, in both directions.
// Hidden grading filters suites by the "echo." prefix.
#define LABSTEST_MAIN
#include <cstdint>

#include "labstest.hpp"
#include "echo_bus.hpp"

using gbecho::WramWithEcho;

TEST(echo, write_through_alias_lands_in_wram) {
    WramWithEcho m;
    m.bus.write(0xE000, 0x42);   // first aliased byte -> C000
    m.bus.write(0xE123, 0x77);   // -> C123
    m.bus.write(0xFDFF, 0x99);   // last aliased byte  -> DDFF
    EXPECT_EQ(m.wram.cells()[0x0000], 0x42);
    EXPECT_EQ(m.wram.cells()[0x0123], 0x77);
    EXPECT_EQ(m.wram.cells()[0x1DFF], 0x99);
}

TEST(echo, read_through_alias_serves_wram) {
    WramWithEcho m;
    m.wram.write(0xC100, 0x11);  // direct WRAM poke
    m.wram.write(0xDDFF, 0x22);
    EXPECT_EQ(m.bus.read(0xE100), 0x11);
    EXPECT_EQ(m.bus.read(0xFDFF), 0x22);
}

TEST(echo, roundtrip_is_transparent_in_both_directions) {
    WramWithEcho m;
    const uint16_t pairs[] = {0xE000, 0xE001, 0xEA5A, 0xEEEE, 0xF7FF, 0xFDFF};
    for (uint16_t echo : pairs) {
        const uint8_t v = static_cast<uint8_t>(echo >> 3) | 0x01;
        m.bus.write(echo, v);                       // via echo
        EXPECT_EQ(m.bus.read(static_cast<uint16_t>(echo - 0x2000)), v);  // via WRAM side
        m.bus.write(static_cast<uint16_t>(echo - 0x2000), static_cast<uint8_t>(~v));  // via WRAM
        EXPECT_EQ(m.bus.read(echo), static_cast<uint8_t>(~v));           // via echo
    }
}

TEST(echo, translation_is_exact_minus_2000_not_wrapped) {
    WramWithEcho m;
    // FDFF aliases DDFF (offset $1DFF). If a wrap or off-by-one sneaks in,
    // DFFF's cell would get hit instead — prove it stays zero.
    m.bus.write(0xFDFF, 0x63);
    EXPECT_EQ(m.wram.cells()[0x1DFF], 0x63);  // DDFF
    EXPECT_EQ(m.wram.cells()[0x1FFF], 0x00);  // DFFF untouched
    EXPECT_EQ(m.wram.cells()[0x1E00], 0x00);  // DE00 untouched too
}

TEST(echo, wram_tail_de00_dfff_has_no_echo_counterpart) {
    WramWithEcho m;
    // FDFF traffic lands on DDFF and must never spill into DE00-DFFF:
    m.bus.write(0xFDFF, 0x66);
    EXPECT_EQ(m.wram.cells()[0x1DFF], 0x66);
    EXPECT_EQ(m.wram.cells()[0x1E00], 0x00);  // DE00 untouched
    EXPECT_EQ(m.wram.cells()[0x1FFF], 0x00);  // DFFF untouched
    // Direct pokes to the tail are real WRAM writes, just unaliased ones.
    m.bus.write(0xDE00, 0x44);
    m.bus.write(0xDFFF, 0x55);
    EXPECT_EQ(m.wram.cells()[0x1E00], 0x44);
    EXPECT_EQ(m.wram.cells()[0x1FFF], 0x55);
}

TEST(echo, alias_is_the_same_cells_not_a_copy) {
    WramWithEcho m;
    // Interleave both paths on one cell and watch each overwrite win.
    m.bus.write(0xC000 + 0x10, 0x01);
    m.bus.write(0xE000 + 0x10, 0x02);
    EXPECT_EQ(m.bus.read(0xC000 + 0x10), 0x02);
    m.bus.write(0xC000 + 0x10, 0x03);
    EXPECT_EQ(m.bus.read(0xE000 + 0x10), 0x03);
}
