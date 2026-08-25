#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "nesdbg.hpp"

using namespace nesrom;

namespace {

std::vector<uint8_t> make_rom(uint8_t prg, uint8_t chr, uint8_t f6) {
    std::vector<uint8_t> rom{'N', 'E', 'S', 0x1A, prg, chr, f6, 0x00,
                             0, 0, 0, 0, 0, 0, 0, 0};
    rom.resize(16 + size_t(prg) * 16384 + size_t(chr) * 8192, 0xFF);
    return rom;
}

}  // namespace

// Regression tests for both seeded bugs. RED until fixed; keep them green
// afterwards and cite them in bug-report.md.

TEST(regression, mirroring_bit_sense_matches_the_header_spec) {
    Header h;
    auto rom = make_rom(1, 1, 0x00);       // flag6 bit0 = 0 -> horizontal
    EXPECT_TRUE(h.parse(rom));
    EXPECT_TRUE(h.mirroring == Mirroring::Horizontal);

    rom[6] = 0x01;                         // bit0 = 1 -> vertical
    EXPECT_TRUE(h.parse(rom));
    EXPECT_TRUE(h.mirroring == Mirroring::Vertical);

    rom[6] = 0x00;                         // (buggy build swaps the two)
    EXPECT_TRUE(h.parse(rom));
}

TEST(regression, nametable_writes_follow_the_header_layout) {
    Header h;
    auto rom = make_rom(1, 1, 0x00);       // horizontal arrangement
    EXPECT_TRUE(h.parse(rom));
    NROM* cart = NROM::create(h, rom);

    cart->ppu_write(0x2005, 0x42);         // table 0
    // Horizontal: tables 0/1 share a bank, so $2405 mirrors $2005...
    EXPECT_EQ(cart->ppu_read(0x2405), 0x42);
    // ...and tables 2/3 hold the other bank, untouched by that write.
    EXPECT_EQ(cart->ppu_read(0x2805), 0x00);
    delete cart;

    rom[6] = 0x01;                         // vertical arrangement
    EXPECT_TRUE(h.parse(rom));
    NROM* v = NROM::create(h, rom);
    v->ppu_write(0x2005, 0x77);
    // Vertical: $2805 pairs with $2005 instead.
    EXPECT_EQ(v->ppu_read(0x2805), 0x77);
    EXPECT_EQ(v->ppu_read(0x2405), 0x00);
    delete v;
}

TEST(regression, dma_debit_is_513_or_514_by_alignment) {
    nesbus::NesBus bus;
    bus.cycles = 1;                        // odd start: plain 513 debit
    const uint64_t t0 = bus.cycles;
    bus.write(0x4014, 0x02);
    EXPECT_EQ(bus.cycles - t0, 514u);      // buggy build reports 513

    nesbus::NesBus bus2;
    const uint64_t t1 = bus2.cycles;       // start at 0: copy begins on an
    bus2.write(0x4014, 0x02);              // odd count -> +1 get cycle
    EXPECT_EQ(bus2.cycles - t1, 515u);     // buggy build reports 513
}
