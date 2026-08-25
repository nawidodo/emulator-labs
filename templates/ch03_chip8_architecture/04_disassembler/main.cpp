#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>

#include "chip8.hpp"

using chip8::Chip8;

namespace {

struct Row {
    uint8_t hi;
    uint8_t lo;
    const char* line;  // exact expected output, address baked in
};

// Table-driven: every implemented mnemonic plus representative fall-throughs.
// Instructions are poked at their natural addresses starting at 0x200.
const Row kRows[] = {
    {0x00, 0xE0, "0200: 00E0  CLS"},
    {0x12, 0x14, "0202: 1214  JP 0x214"},
    {0x6A, 0x42, "0204: 6A42  LD VA, 0x42"},
    {0x75, 0x2A, "0206: 752A  ADD V5, 0x2A"},
    {0xA2, 0x2A, "0208: A22A  LD I, 0x22A"},
    // Not implemented in this chapter: raw DW pseudo-op keeps bytes visible.
    {0xD0, 0x15, "020A: D015  DW 0xD015"},
    {0x00, 0xEE, "020C: 00EE  DW 0x00EE"},
    {0xF0, 0x29, "020E: F029  DW 0xF029"},
};

}  // namespace

TEST(disasm, table_driven_formatting) {
    Chip8 c;
    c.reset();
    for (size_t n = 0; n < sizeof(kRows) / sizeof(kRows[0]); ++n) {
        const uint16_t addr = static_cast<uint16_t>(0x200 + 2 * n);
        c.poke_mem(addr, kRows[n].hi);
        c.poke_mem(static_cast<uint16_t>(addr + 1), kRows[n].lo);
    }
    for (size_t n = 0; n < sizeof(kRows) / sizeof(kRows[0]); ++n) {
        const uint16_t addr = static_cast<uint16_t>(0x200 + 2 * n);
        const std::string got = c.disassemble(addr);
        EXPECT_EQ(got, std::string(kRows[n].line));
    }
}

TEST(disasm, disassembles_whole_demo_rom) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x00, 0xE0, 0x60, 0x42, 0x71, 0x17};
    c.load(rom);
    EXPECT_EQ(c.disassemble(0x200), "0200: 00E0  CLS");
    EXPECT_EQ(c.disassemble(0x202), "0202: 6042  LD V0, 0x42");
    EXPECT_EQ(c.disassemble(0x204), "0204: 7117  ADD V1, 0x17");
}

TEST(disasm, fields_reach_the_right_places) {
    // X from the wrong nibble would print V3 instead of VB here.
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x7B, 0x03};
    c.load(rom);
    EXPECT_EQ(c.disassemble(0x200), "0200: 7B03  ADD VB, 0x03");
}
