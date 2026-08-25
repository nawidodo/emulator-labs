#define LABSTEST_MAIN
#include "labstest.hpp"
#include "region.hpp"

using namespace gba;


TEST(region, decode_all_nine_regions) {
    EXPECT_EQ(route(0x00000000u), Region::Bios);
    EXPECT_EQ(route(0x00003FFCu), Region::Bios);
    EXPECT_EQ(route(0x02000000u), Region::Ewram);
    EXPECT_EQ(route(0x0203FFFFu), Region::Ewram);
    EXPECT_EQ(route(0x03007FF8u), Region::Iwram);
    EXPECT_EQ(route(0x04000000u), Region::Io);
    EXPECT_EQ(route(0x05000000u), Region::Palette);
    EXPECT_EQ(route(0x06000000u), Region::Vram);
    EXPECT_EQ(route(0x070003F8u), Region::Oam);
    EXPECT_EQ(route(0x08000000u), Region::RomWs0);
    EXPECT_EQ(route(0x0A000000u), Region::RomWs1);
    EXPECT_EQ(route(0x0C000000u), Region::RomWs2);
    EXPECT_EQ(route(0x0E000000u), Region::Sram);
}

TEST(region, rom_chip_split_and_mirrors) {
    // Each ROM chip owns two 32 M... two top-byte mirrors.
    EXPECT_EQ(route(0x09FFFFFFu), Region::RomWs0);
    EXPECT_EQ(route(0x0B555555u), Region::RomWs1);
    EXPECT_EQ(route(0x0D000000u), Region::RomWs2);
    EXPECT_EQ(route(0x0F000000u), Region::Sram);   // SRAM mirror at 0x0F
}

TEST(region, open_bus_ranges) {
    EXPECT_EQ(route(0x10000000u), Region::OpenBus);
    EXPECT_EQ(route(0x04800000u), Region::OpenBus);
    EXPECT_EQ(route(0x00010000u), Region::OpenBus);  // BIOS hole (unmapped)
}

TEST(region, ewram_iwram_mirrors) {
    EXPECT_EQ(ewram_canonical(0x02040004u), 0x0004u);  // +256K wraps
    EXPECT_EQ(ewram_canonical(0x02F80010u), 0x80010u & 0x3FFFFu);
    EXPECT_EQ(iwram_canonical(0x03008000u), 0u);       // +32K wraps to base
    EXPECT_EQ(iwram_canonical(0x03007FF0u), 0x7FF0u);
}

TEST(region, vram_discontinuity) {
    // BG half: direct below 64K.
    EXPECT_EQ(vram_canonical(0x06000010u), 0x10u);
    // OBJ half: direct too — NO naive 64K fold.
    EXPECT_NE(vram_canonical(0x06014444u), vram_canonical(0x06004444u));
    EXPECT_EQ(vram_canonical(0x06014444u), 0x14444u);
    // The top hole reflects the BG upper half.
    EXPECT_EQ(vram_canonical(0x06018888u), 0x8888u);
    EXPECT_EQ(vram_canonical(0x0601FFFFu), 0xFFFFu);
    // 128 K boundary wraps everything.
    EXPECT_EQ(vram_canonical(0x06020004u), 0x4u);
}

TEST(hidden, region_hidden_sweep) {
    // Every address in the ROM mirror space routes to some chip; the
    // three chips tile 08-0D exactly.
    for (uint32_t top = 0x08; top <= 0x0D; ++top) {
        const Region r = route(top << 24 | 0x1234u);
        EXPECT_TRUE(r == Region::RomWs0 || r == Region::RomWs1 ||
                    r == Region::RomWs2);
    }
    for (uint32_t off = 0; off < 0x20000u; off += 0x4321u) {
        const uint32_t c = vram_canonical(0x06000000u + off);
        EXPECT_TRUE(c < 0x18000u);
    }
}
