#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <vector>

#include "neschr.hpp"

using namespace neschr;

namespace {

std::vector<uint8_t> make_rom(uint8_t prg, uint8_t chr, uint8_t f6) {
    std::vector<uint8_t> rom{'N', 'E', 'S', 0x1A, prg, chr, f6, 0x00,
                             0, 0, 0, 0, 0, 0, 0, 0};
    rom.resize(16 + size_t(prg) * 16384 + size_t(chr) * 8192, 0xFF);
    return rom;
}

ChrRamNROM* build(uint8_t chr_banks) {
    Header h;
    const auto rom = make_rom(1, chr_banks, 0x00);
    EXPECT_TRUE(h.parse(rom));   // no-op in release path; keeps rig honest
    return ChrRamNROM::create(h, rom);
}

}  // namespace

TEST(unseen, chrram_flag_follows_the_bank_count) {
    ChrRamNROM* ram = build(0);
    EXPECT_TRUE(ram->chr_is_ram);
    delete ram;

    ChrRamNROM* rom = build(1);
    EXPECT_FALSE(rom->chr_is_ram);
    delete rom;
}

TEST(unseen, chrram_reads_and_writes_round_trip) {
    ChrRamNROM* cart = build(0);
    cart->ppu_write(0x0000, 0xA7);
    cart->ppu_write(0x1FFF, 0x3C);     // top of the CHR window
    EXPECT_EQ(cart->ppu_read(0x0000), 0xA7);
    EXPECT_EQ(cart->ppu_read(0x1FFF), 0x3C);
    delete cart;
}

TEST(unseen, chrram_is_backed_by_an_8k_ram_cell) {
    ChrRamNROM* cart = build(0);
    cart->chr_ram[0x1ABC] = 0x55;      // the spec'd storage is one 8KB RAM
    EXPECT_EQ(cart->ppu_read(0x1ABC), 0x55);   // window ends at $1FFF
    cart->ppu_write(0x0F00, 0x77);
    EXPECT_EQ(cart->chr_ram[0x0F00], 0x77);
    delete cart;
}

TEST(unseen, chr_rom_still_drops_writes) {
    ChrRamNROM* cart = build(1);
    cart->chr_rom.assign(8192, 0x11);
    cart->chr_rom[0x0022] = 0x33;
    cart->ppu_write(0x0022, 0xEE);     // must NOT stick
    EXPECT_EQ(cart->ppu_read(0x0022), 0x33);
    delete cart;
}

TEST(unseen, ciram_side_untouched_by_chr_routing) {
    ChrRamNROM* cart = build(0);
    cart->ppu_write(0x2005, 0x77);     // nametable write still works
    EXPECT_EQ(cart->ppu_read(0x2405), 0x77);   // horizontal mirror pair
    delete cart;
}
