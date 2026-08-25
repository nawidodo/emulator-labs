#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "nesrom.hpp"

using namespace nesrom;

namespace {

// Minimal iNES file builder: header + PRG (+ CHR).
std::vector<uint8_t> make_rom(uint8_t prg, uint8_t chr, uint8_t f6,
                              const std::vector<uint8_t>& prg_data) {
    std::vector<uint8_t> rom{'N', 'E', 'S', 0x1A, prg, chr, f6, 0x00,
                             0, 0, 0, 0, 0, 0, 0, 0};
    rom.insert(rom.end(), prg_data.begin(), prg_data.end());
    rom.resize(16 + size_t(prg) * 16384 + size_t(chr) * 8192, 0xFF);
    return rom;
}

}  // namespace

TEST(ines, parses_bank_counts_and_magic) {
    Header h;
    EXPECT_FALSE(h.parse({}));
    EXPECT_FALSE(h.parse({'N', 'E', 'S', 'X', 1, 1, 0, 0}));
    const auto rom = make_rom(2, 1, 0x00, {0xEA});
    EXPECT_TRUE(h.parse(rom));
    EXPECT_EQ(h.prg_banks, 2);
    EXPECT_EQ(h.chr_banks, 1);
}

TEST(ines, mapper_low_nibble_comes_from_flag6_high_bits) {
    Header h;
    auto rom = make_rom(1, 0, 0x00, {});   // mapper 0
    EXPECT_TRUE(h.parse(rom));
    EXPECT_EQ(h.mapper, 0x00);

    rom[6] = 0x10;                         // mapper low nibble = 1
    EXPECT_TRUE(h.parse(rom));
    EXPECT_EQ(h.mapper, 0x01);

    rom[7] = 0x20;                         // mapper high bits (8-11) = 2
    EXPECT_TRUE(h.parse(rom));
    EXPECT_EQ(h.mapper, 0x21);
}

TEST(ines, mirroring_bit_selects_horizontal_or_vertical) {
    Header h;
    auto rom = make_rom(1, 1, 0x00, {});   // bit0 = 0
    EXPECT_TRUE(h.parse(rom));
    EXPECT_TRUE(h.mirroring == Mirroring::Horizontal);

    rom[6] = 0x01;                         // bit0 = 1
    EXPECT_TRUE(h.parse(rom));
    EXPECT_TRUE(h.mirroring == Mirroring::Vertical);

    rom[6] |= 0x08;                        // four-screen overrides
    EXPECT_TRUE(h.parse(rom));
    EXPECT_TRUE(h.mirroring == Mirroring::FourScreen);
}

TEST(mirror, vertical_mirroring_pairs_tables_0_2_and_1_3) {
    // Vertical (CIRAM A10 = PPU A10): side-scrolling layout. $2000 and
    // $2800 land in the same bank; so do $2400 and $2C00.
    EXPECT_EQ(mirror_translate(0x2000, Mirroring::Vertical), 0x000);
    EXPECT_EQ(mirror_translate(0x2800, Mirroring::Vertical), 0x000);
    EXPECT_EQ(mirror_translate(0x2400, Mirroring::Vertical), 0x400);
    EXPECT_EQ(mirror_translate(0x2C00, Mirroring::Vertical), 0x400);
}

TEST(mirror, horizontal_mirroring_pairs_tables_0_1_and_2_3) {
    // Horizontal (CIRAM A10 = PPU A11): vertically-scrolling layout.
    // $2000/$2400 share a bank; $2800/$2C00 share the other.
    EXPECT_EQ(mirror_translate(0x2000, Mirroring::Horizontal), 0x000);
    EXPECT_EQ(mirror_translate(0x2400, Mirroring::Horizontal), 0x000);
    EXPECT_EQ(mirror_translate(0x2800, Mirroring::Horizontal), 0x400);
    EXPECT_EQ(mirror_translate(0x2C00, Mirroring::Horizontal), 0x400);
    // Scrolling past $2FFF mirrors back into the first range.
    EXPECT_EQ(mirror_translate(0x3004, Mirroring::Horizontal),
              mirror_translate(0x2004, Mirroring::Horizontal));
}

TEST(nrom, single_bank_mirrors_across_the_32k_window) {
    Header h;
    std::vector<uint8_t> prg(16384, 0xEE);
    prg[0] = 0x11;                       // $8000
    prg[16383] = 0x22;                   // $BFFF
    const auto rom = make_rom(1, 0, 0x00, {});
    EXPECT_TRUE(h.parse(rom));
    NROM* cart = NROM::create(h, rom);
    cart->prg = prg;

    EXPECT_EQ(cart->cpu_read(0x8000), 0x11);
    EXPECT_EQ(cart->cpu_read(0xBFFF), 0x22);
    EXPECT_EQ(cart->cpu_read(0xC000), 0x11);   // mirror of $8000
    EXPECT_EQ(cart->cpu_read(0xFFFF), 0x22);   // mirror of $BFFF
    delete cart;
}

TEST(nrom, two_banks_map_linearly_and_writes_dont_stick) {
    Header h;
    std::vector<uint8_t> prg(32768, 0x00);
    prg[0] = 0xAA;                       // $8000
    prg[16384] = 0xBB;                   // $C000 — distinct second bank!
    const auto rom = make_rom(2, 0, 0x00, {});
    EXPECT_TRUE(h.parse(rom));
    NROM* cart = NROM::create(h, rom);
    cart->prg = prg;

    EXPECT_EQ(cart->cpu_read(0x8000), 0xAA);
    EXPECT_EQ(cart->cpu_read(0xC000), 0xBB);   // NOT a mirror this time

    cart->cpu_write(0x8000, 0x99);
    EXPECT_EQ(cart->cpu_read(0x8000), 0xAA);   // PRG is ROM
    delete cart;
}

TEST(nrom, ppu_chr_reads_and_ciram_respect_mirroring) {
    Header h;
    const auto rom = make_rom(1, 1, 0x01, {});   // vertical
    EXPECT_TRUE(h.parse(rom));
    NROM* cart = NROM::create(h, rom);
    cart->chr.assign(8192, 0x00);
    cart->chr[0x0015] = 0x77;

    EXPECT_EQ(cart->ppu_read(0x0015), 0x77);

    cart->ppu_write(0x2005, 0x42);               // nametable 0
    EXPECT_EQ(cart->ppu_read(0x2005), 0x42);
    EXPECT_EQ(cart->ppu_read(0x2805), 0x42);     // vertical pair of table 0
    EXPECT_EQ(cart->ppu_read(0x2405), 0x00);     // separate CIRAM bank
    delete cart;
}
