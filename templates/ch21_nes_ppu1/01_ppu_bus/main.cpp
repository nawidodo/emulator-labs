#define LABSTEST_MAIN
#include "labstest.hpp"

#include "ppu_bus.hpp"

using nes21bus::Mirroring;
using nes21bus::PpuBus;

TEST(nes21bus, vertical_mirroring_shares_page_on_bit10) {
    PpuBus ppu(Mirroring::Vertical);
    ppu.write(0x2005, 0xAB);
    EXPECT_EQ(ppu.read(0x2805), 0xAB);   // $2800 mirrors $2000
    EXPECT_EQ(ppu.read(0x2405), 0x00);   // $2400 is the other page
    EXPECT_EQ(ppu.read(0x2C05), 0x00);
}

TEST(nes21bus, horizontal_mirroring_shares_page_on_bit11) {
    PpuBus ppu(Mirroring::Horizontal);
    ppu.write(0x2005, 0xCD);
    EXPECT_EQ(ppu.read(0x2405), 0xCD);   // $2400 mirrors $2000
    EXPECT_EQ(ppu.read(0x2805), 0x00);
}

TEST(nes21bus, four_screen_has_four_independent_pages) {
    PpuBus ppu(Mirroring::FourScreen);
    for (uint16_t base = 0x2000; base < 0x3000; base += 0x0400)
        ppu.write(uint16_t(base + 1), uint8_t(base >> 8));
    EXPECT_EQ(ppu.read(0x2001), 0x20);
    EXPECT_EQ(ppu.read(0x2401), 0x24);
    EXPECT_EQ(ppu.read(0x2801), 0x28);
    EXPECT_EQ(ppu.read(0x2C01), 0x2C);
}

TEST(nes21bus, region_3000_3eff_folds_onto_nametables) {
    PpuBus ppu(Mirroring::Vertical);
    ppu.write(0x3000, 0x5A);
    EXPECT_EQ(ppu.read(0x2000), 0x5A);
    ppu.write(0x3EFF, 0xA5);
    EXPECT_EQ(ppu.read(0x2EFF), 0xA5);
}

TEST(nes21bus, pattern_tables_are_flat) {
    PpuBus ppu(Mirroring::Horizontal);
    ppu.chr[0x0000] = 0x11;
    ppu.chr[0x1FFF] = 0x22;
    EXPECT_EQ(ppu.read(0x0000), 0x11);
    EXPECT_EQ(ppu.read(0x1FFF), 0x22);
}

TEST(nes21bus, palette_sprite_entry_zero_mirrors_backdrop) {
    PpuBus ppu(Mirroring::Horizontal);
    ppu.write(0x3F10, 0x1F);
    EXPECT_EQ(ppu.read(0x3F00), 0x1F);   // universal backdrop
    ppu.write(0x3F14, 0x21);
    EXPECT_EQ(ppu.read(0x3F04), 0x21);
    ppu.write(0x3F18, 0x23);
    EXPECT_EQ(ppu.read(0x3F08), 0x23);
    ppu.write(0x3F1C, 0x25);
    EXPECT_EQ(ppu.read(0x3F0C), 0x25);
}

TEST(nes21bus, palette_normal_entries_are_direct) {
    PpuBus ppu(Mirroring::Horizontal);
    ppu.write(0x3F05, 0x16);
    EXPECT_EQ(ppu.read(0x3F05), 0x16);   // bg palette 1, color 1
    EXPECT_EQ(ppu.read(0x3F15), 0x00);   // sprite palette 1 entry 1: separate
}

TEST(nes21bus, palette_mirrors_through_3fff) {
    PpuBus ppu(Mirroring::Horizontal);
    ppu.write(0x3F24, 0x2A);             // $3F24 -> $3F04
    EXPECT_EQ(ppu.read(0x3F04), 0x2A);
    ppu.write(0x3FFF, 0x30);             // $3FFF -> $3F1F
    EXPECT_EQ(ppu.read(0x3F1F), 0x30);
}
