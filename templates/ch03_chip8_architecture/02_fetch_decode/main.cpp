#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "chip8.hpp"
#include "decode.hpp"

using chip8::Chip8;
using chip8::decode::fetch_opcode;

TEST(fetch, big_endian_pairs) {
    const uint8_t a[] = {0xA2, 0x2A};
    EXPECT_EQ(fetch_opcode(a), 0xA22A);
    const uint8_t b[] = {0x00, 0xE0};
    EXPECT_EQ(fetch_opcode(b), 0x00E0);
    // Endianness trap: low byte first would give 0x01FF here.
    const uint8_t c[] = {0xFF, 0x01};
    EXPECT_EQ(fetch_opcode(c), 0xFF01);
}

namespace {

struct Fields {
    uint16_t op;
    uint16_t nnn;
    uint8_t n;
    uint8_t nn;
    uint8_t x;
    uint8_t y;
};

// One hand-decoded sample per major opcode prefix (0-D), plus extremes.
const Fields kSamples[] = {
    {0x0000, 0x000, 0x0, 0x00, 0x0, 0x0},
    {0xD015, 0x015, 0x5, 0x15, 0x0, 0x1},
    {0x00E0, 0x0E0, 0x0, 0xE0, 0x0, 0xE},
    {0x1110, 0x110, 0x0, 0x10, 0x1, 0x1},
    {0x2ABC, 0xABC, 0xC, 0xBC, 0xA, 0xB},
    {0x3007, 0x007, 0x7, 0x07, 0x0, 0x0},
    {0x40D8, 0x0D8, 0x8, 0xD8, 0x0, 0xD},
    {0x5230, 0x230, 0x0, 0x30, 0x2, 0x3},
    {0x6A42, 0xA42, 0x2, 0x42, 0xA, 0x4},
    {0x7FE3, 0xFE3, 0x3, 0xE3, 0xF, 0xE},
    {0x8124, 0x124, 0x4, 0x24, 0x1, 0x2},
    {0x9AB0, 0xAB0, 0x0, 0xB0, 0xA, 0xB},
    {0xA22A, 0x22A, 0xA, 0x2A, 0x2, 0x2},
    {0xB123, 0x123, 0x3, 0x23, 0x1, 0x2},
    {0xC80F, 0x80F, 0xF, 0x0F, 0x8, 0x0},
    {0xE0A1, 0x0A1, 0x1, 0xA1, 0x0, 0xA},
    {0xF129, 0x129, 0x9, 0x29, 0x1, 0x2},
};

}  // namespace

TEST(decode, field_extraction_over_sample_opcodes) {
    for (const auto& s : kSamples) {
        EXPECT_EQ(chip8::decode::nnn(s.op), s.nnn);
        EXPECT_EQ(chip8::decode::n(s.op), s.n);
        EXPECT_EQ(chip8::decode::nn(s.op), s.nn);
        EXPECT_EQ(chip8::decode::x(s.op), s.x);
        EXPECT_EQ(chip8::decode::y(s.op), s.y);
    }
}

// Exhaustive sweep over every representable opcode: the five extractors must
// exactly partition the word — prefix, X (bits 11..8), Y (bits 7..4) and N
// (bits 3..0) rebuild the original opcode bit for bit. Any nibble in the
// wrong place breaks the round trip.
TEST(decode, exhaustive_fields_rebuild_every_opcode) {
    for (uint32_t op = 0; op <= 0xFFFF; ++op) {
        const uint16_t o = static_cast<uint16_t>(op);
        const uint16_t rebuilt = static_cast<uint16_t>(
            (o & 0xF000) | (chip8::decode::x(o) << 8) |
            (chip8::decode::y(o) << 4) | chip8::decode::n(o));
        EXPECT_EQ(rebuilt, o);
        if (::labstest::failures() > 0) return;  // keep first failure visible
    }
}

TEST(decode, exhaustive_low_fields_partition_word) {
    for (uint32_t op = 0; op <= 0xFFFF; ++op) {
        const uint16_t o = static_cast<uint16_t>(op);
        // nnn is exactly the word with the prefix nibble removed.
        const uint16_t prefix = static_cast<uint16_t>(o & 0xF000);
        EXPECT_EQ(chip8::decode::nnn(o), o & 0x0FFFu);
        // nn and n are truncations of nnn.
        EXPECT_EQ(chip8::decode::nn(o), chip8::decode::nnn(o) & 0xFFu);
        EXPECT_EQ(chip8::decode::n(o), chip8::decode::nnn(o) & 0xFu);
        EXPECT_EQ(static_cast<uint16_t>(prefix | chip8::decode::nnn(o)), o);
        if (::labstest::failures() > 0) return;
    }
}

TEST(step, fetches_and_advances_pc_by_two) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x60, 0x42, 0x71, 0x17, 0xA2, 0x06};
    c.load(rom);

    auto r = c.step();
    EXPECT_EQ(r.pc, 0x202);
    EXPECT_EQ(r.cycles, 1);
    EXPECT_EQ(c.last_op(), 0x6042);

    r = c.step();
    EXPECT_EQ(r.pc, 0x204);
    EXPECT_EQ(c.last_op(), 0x7117);

    r = c.step();
    EXPECT_EQ(r.pc, 0x206);
    EXPECT_EQ(c.last_op(), 0xA206);
}

TEST(step, fetch_beyond_rom_reads_zeroed_memory) {
    Chip8 c;
    c.reset();
    auto r = c.step();
    EXPECT_EQ(c.last_op(), 0x0000);
    EXPECT_EQ(r.pc, 0x202);
}
