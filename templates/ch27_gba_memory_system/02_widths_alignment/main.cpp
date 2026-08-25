#define LABSTEST_MAIN
#include "labstest.hpp"
#include "widths.hpp"

using namespace gba;

TEST(widths, aligned_reads_of_all_widths) {
    WidthBus bus;
    bus.write(0x100, 4, 0x11223344u);
    EXPECT_EQ(bus.read(0x100, 4), 0x11223344u);
    EXPECT_EQ(bus.read(0x100, 2), 0x3344u);
    EXPECT_EQ(bus.read(0x102, 2), 0x1122u);
    EXPECT_EQ(bus.read(0x103, 1), 0x11u);
}

TEST(widths, unaligned_word_read_rotates) {
    WidthBus bus;
    // Aligned word at 0x200: bytes LE = 44 33 22 11.
    bus.write(0x200, 4, 0x11223344u);
    // LDR from 0x201: rotate right by 8 -> requested byte (0x33) lands low.
    EXPECT_EQ(bus.read(0x201, 4), 0x44112233u);
    EXPECT_EQ(bus.read(0x202, 4), 0x33441122u);
    // Byte reads never rotate: the addressed byte is the value.
    EXPECT_EQ(bus.read(0x201, 1), bus.mem[0x201]);
}

TEST(widths, unaligned_halfword_read_rotates_on_16bit_bus) {
    WidthBus bus;
    bus.write(0x300, 2, 0xAABBu);          // aligned halfword
    bus.write(0x302, 2, 0xCCDDu);
    // Odd halfword address: containing aligned halfword (0xAABB) rotated
    // right through 32 bits by (addr & 1) * 8.
    EXPECT_EQ(bus.read(0x301, 2), (0xAABBu >> 8) | (0xAABBu << 24));
}

TEST(widths, writes_store_exact_bytes) {
    WidthBus bus;
    bus.write(0x400, 4, 0xDEADBEEFu);
    bus.write(0x402, 2, 0x1234u);          // overwrites top half
    EXPECT_EQ(bus.read(0x400, 4), 0x1234BEEFu);
    bus.write(0x406, 1, 0x7Fu);
    EXPECT_EQ(bus.read(0x406, 1), 0x7Fu);
}

TEST(widths, wraparound_at_region_end) {
    WidthBus bus;
    bus.write(WidthBus::kSize - 1, 4, 0xA1B2C3D4u);  // spans past end: wraps
    EXPECT_EQ(bus.read(0x7FFF, 1), 0xD4u);
    EXPECT_EQ(bus.read(0x0000, 1), 0xC3u); // wrapped to region start
}

TEST(hidden, widths_hidden_rotation_matrix) {
    WidthBus bus;
    for (unsigned i = 0; i < 4; ++i)
        bus.write(i * 4, 4, 0x03020100u + i * 0x04040404u);
    // For every misalignment the addressed byte must appear in bits 0-7.
    for (uint32_t addr = 0; addr < 16; ++addr) {
        const uint32_t v = bus.read(addr, 4);
        EXPECT_EQ(v & 0xFFu, bus.mem[addr]);
    }
}
