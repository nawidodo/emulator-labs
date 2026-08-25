#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include "bus.hpp"

using snesbus::Bus;
using snesbus::kNotWram;
using snesbus::Region;
using snesbus::region_of;
using snesbus::rom_index;
using snesbus::wram_offset;

namespace {

// Representative banks: both system halves, both WRAM banks, the fastROM
// mirror range and one undecoded bank.
constexpr uint8_t kProbeBanks[] = {0x00, 0x01, 0x15, 0x3F, 0x40,
                                   0x7E, 0x7F, 0x80, 0xA5, 0xBF};

Region expected_region(uint32_t bank, uint32_t off) {
    if (bank == 0x7E || bank == 0x7F) {
        return Region::WRAM_DIRECT;
    }
    if (bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) {
        if (off < 0x2000) return Region::WRAM_MIRROR;
        if ((off & 0xFF00u) == 0x2100u) return Region::PPU_REGS;
        if (off >= 0x8000) return Region::CART_ROM;
    }
    return Region::OPEN_BUS;  // includes all of bank $40-$7D in our model
}

}  // namespace

TEST(decode, representative_banks_over_offsets) {
    for (uint32_t bank : kProbeBanks) {
        for (uint32_t off = 0; off < 0x10000u; off += 0x111u) {
            const uint32_t addr = (bank << 16) | off;
            EXPECT_EQ(region_of(addr), expected_region(bank, off));
            EXPECT_EQ(wram_offset(addr) != kNotWram,
                      expected_region(bank, off) == Region::WRAM_DIRECT ||
                          expected_region(bank, off) == Region::WRAM_MIRROR);
        }
    }
}

TEST(decode, undecoded_bank_is_open_bus) {
    // Banks $40-$7D decode to open bus everywhere in this model, even at
    // ROM-shaped offsets.
    for (uint32_t off = 0; off < 0x10000u; off += 0x1234u) {
        EXPECT_EQ(region_of(0x00400000u | off), Region::OPEN_BUS);
    }
}

TEST(wram, mirror_aliases_low_8k_only) {
    Bus bus;
    bus.write(0x7E0123, 0x5A);
    EXPECT_EQ(bus.read(0x00000123), 0x5A);
    bus.write(0x3F1FFF, 0xC3);
    EXPECT_EQ(bus.read(0x7E1FFF), 0xC3);

    // The upper half of a WRAM bank has no mirror window in $00-$3F.
    bus.write(0x7E8000, 0x77);
    EXPECT_EQ(bus.read(0x00008000), 0xFF);  // open bus float, not WRAM
    EXPECT_EQ(wram_offset(0x00008000), kNotWram);
}

TEST(wram, full_direct_reachability) {
    Bus bus;
    // Walk all 128 KiB through the direct window.
    for (uint32_t a = 0x7E0000; a < 0x800000; a += 0x1001u) {
        bus.write(a, static_cast<uint8_t>(a * 7 + 3));
    }
    for (uint32_t a = 0x7E0000; a < 0x800000; a += 0x1001u) {
        EXPECT_EQ(bus.read(a), static_cast<uint8_t>(a * 7 + 3));
    }
    // Bank $7F is the second 64 KiB half, not a mirror of $7E.
    bus.write(0x7F4000, 0xEE);
    EXPECT_EQ(bus.read(0x7F4000), 0xEE);
    EXPECT_EQ(bus.read(0x7E4000), 0x00);
}

TEST(ppuregs, page_repeats_across_system_banks) {
    Bus bus;
    bus.write(0x002142, 0xAB);
    EXPECT_EQ(bus.read(0x002142), 0xAB);
    // Same $21xx page answers in every system bank and its mirror range.
    EXPECT_EQ(bus.read(0x012142), 0xAB);
    EXPECT_EQ(bus.read(0x3F2142), 0xAB);
    EXPECT_EQ(bus.read(0x802142), 0xAB);
    EXPECT_EQ(bus.read(0xBF2142), 0xAB);
    // Adjacent pages do not decode as PPU ports.
    EXPECT_EQ(region_of(0x002200), Region::OPEN_BUS);
}

TEST(rom, lorom_pages_and_mirror_banks) {
    Bus bus;
    bus.rom.assign(4 * 0x8000, 0);
    for (size_t i = 0; i < bus.rom.size(); ++i) {
        bus.rom[i] = static_cast<uint8_t>(i * 13 + 7);
    }

    EXPECT_EQ(bus.read(0x008001), bus.rom[1]);
    EXPECT_EQ(bus.read(0x03FFFF), bus.rom[(0x03u << 15) | 0x7FFF]);
    EXPECT_EQ(bus.read(0x038000), bus.rom[0x18000]);
    // $80-$BF mirrors $00-$3F byte-for-byte.
    EXPECT_EQ(bus.read(0x808001), bus.rom[1]);
    EXPECT_EQ(bus.read(0x838000), bus.rom[0x18000]);
    EXPECT_EQ(rom_index(0x808123), rom_index(0x008123));
}

TEST(rom, writes_dropped_and_short_image_open) {
    Bus bus;
    bus.rom.assign(0x8000, 0x11);
    bus.write(0x008000, 0xFF);  // must not modify ROM
    EXPECT_EQ(bus.read(0x008000), 0x11);

    // Offsets past the image end float like open bus.
    Bus small;
    small.rom.assign(0x100, 0x22);
    EXPECT_EQ(small.read(0x00C000), 0xFF);
    EXPECT_EQ(small.read(0x0080FF), 0x22);  // still inside the tiny page
}

TEST(timing, fast_rom_halves_mirror_access) {
    Bus bus;
    bus.fast_rom = false;
    EXPECT_EQ(bus.cycles_for_access(0x808000), 8);
    EXPECT_EQ(bus.cycles_for_access(0x008000), 8);
    EXPECT_EQ(bus.cycles_for_access(0x7E1234), 8);
    EXPECT_EQ(bus.cycles_for_access(0x002100), 8);

    bus.fast_rom = true;  // $420D bit 0
    EXPECT_EQ(bus.cycles_for_access(0x808000), 6);
    EXPECT_EQ(bus.cycles_for_access(0xBFFFFF), 6);
    EXPECT_EQ(bus.cycles_for_access(0x008000), 8);  // slow window unaffected
    EXPECT_EQ(bus.cycles_for_access(0x7E0002), 8);
}
