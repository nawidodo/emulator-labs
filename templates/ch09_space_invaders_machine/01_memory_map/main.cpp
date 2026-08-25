#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include "bus.hpp"

#include <vector>

// Memory-map contract tests. The mirroring-absence rules are first-class
// citizens here: real boards alias nothing, and a decoder that "helpfully"
// wraps or mirrors ranges will fail these.

using namespace si;

namespace {

// 8 KiB pseudo-random image with a distinct byte per address.
std::vector<uint8_t> test_image() {
    std::vector<uint8_t> img(kRomSize);
    uint16_t lfsr = 0xACE1;
    for (size_t i = 0; i < kRomSize; ++i) {
        lfsr = uint16_t((lfsr >> 1) ^ (uint16_t(0 - (lfsr & 1)) & 0xB400));
        img[i] = uint8_t(lfsr);
    }
    return img;
}

struct Board {
    AddressDecoder bus;
    RomDevice rom;
    RamDevice ram;
    VramDevice vram;

    explicit Board(const std::vector<uint8_t>& image = test_image()) {
        for (size_t off = 0; off < kRomSize; off += kRomBank)
            rom.load_bank(int(off / kRomBank), image.data() + off, kRomBank);
        bus.attach(kRomBase, uint16_t(kRomBase + kRomSize - 1), &rom);
        bus.attach(kRamBase, uint16_t(kRamBase + kRamSize - 1), &ram);
        bus.attach(kVramBase, uint16_t(kVramBase + kVramSize - 1), &vram);
    }
};

}  // namespace

TEST(rom, four_banks_load_linearly_and_read_back) {
    const auto img = test_image();
    Board b(img);
    // Spot-check every bank boundary plus the extremes.
    EXPECT_EQ(b.bus.read(0x0000), img[0x0000]);
    EXPECT_EQ(b.bus.read(0x07FF), img[0x07FF]);   // end of bank 0
    EXPECT_EQ(b.bus.read(0x0800), img[0x0800]);   // start of bank 1
    EXPECT_EQ(b.bus.read(0x1000), img[0x1000]);   // bank 2 middle
    EXPECT_EQ(b.bus.read(0x1FFF), img[0x1FFF]);   // last ROM byte
}

TEST(rom, short_bank_pads_with_erased_ff) {
    RomDevice rom;
    const uint8_t half[kRomBank / 2] = {0x11, 0x22};
    rom.load_bank(2, half, sizeof half);          // only 1 KiB of 2 KiB
    EXPECT_EQ(rom.read(0x0000), 0x00);            // banks 0/1 untouched
    EXPECT_EQ(rom.read(uint16_t(2 * kRomBank)), 0x11);
    EXPECT_EQ(rom.read(uint16_t(2 * kRomBank + 1)), 0x22);
    EXPECT_EQ(rom.read(uint16_t(2 * kRomBank + kRomBank / 2)), 0xFF);
    EXPECT_EQ(rom.read(uint16_t(3 * kRomBank - 1)),
              0xFF);                              // padded tail is erased
}

TEST(rom, writes_are_ignored_everywhere) {
    Board b;
    b.bus.write(0x0100, 0xAA);
    b.bus.write(0x1FFF, 0xBB);
    EXPECT_NE(b.bus.read(0x0100), 0xAA);          // unchanged image
    EXPECT_NE(b.bus.read(0x1FFF), 0xBB);
}

TEST(ram, full_range_roundtrip) {
    Board b;
    for (uint16_t off = 0; off < kRamSize; ++off)
        b.bus.write(uint16_t(kRamBase + off), uint8_t(off * 7 + 1));
    for (uint16_t off = 0; off < kRamSize; ++off)
        EXPECT_EQ(b.bus.read(uint16_t(kRamBase + off)),
                  uint8_t(off * 7 + 1));
}

TEST(ram, no_mirroring_inside_the_window) {
    Board b;
    b.bus.write(0x2000, 0x5A);
    EXPECT_NE(b.bus.read(0x2100), 0x5A);          // no +0x100 mirror
    b.bus.write(0x23FF, 0xC3);
    EXPECT_NE(b.bus.read(0x2000), 0xC3);          // ends are independent
}

TEST(vram, full_range_roundtrip_no_wraparound) {
    Board b;
    b.bus.write(kVramBase, 0x11);
    b.bus.write(0x3FFF, 0x99);
    EXPECT_EQ(b.bus.read(kVramBase), 0x11);
    EXPECT_EQ(b.bus.read(0x3FFF), 0x99);

    // Saturate the whole window with distinct values; the endpoints must
    // not interfere — a wraparound would overwrite the first byte.
    for (uint32_t a = 0; a < kVramSize; ++a)
        b.bus.write(uint16_t(kVramBase + a), uint8_t(a >> 3));
    EXPECT_EQ(b.bus.read(kVramBase), 0x00);
    EXPECT_EQ(b.bus.read(0x3FFF), uint8_t(0x1BFF >> 3));
}

TEST(map, ram_and_vram_do_not_alias_each_other) {
    Board b;
    b.bus.write(0x2000, 0x77);                    // RAM
    EXPECT_NE(b.bus.read(0x2400), 0x77);          // must not appear in VRAM
    b.bus.write(0x2400, 0xEE);                    // VRAM
    EXPECT_NE(b.bus.read(0x2000), 0xEE);          // nor the other way
}

TEST(map, every_address_routes_to_its_own_device) {
    const auto img = test_image();
    Board b(img);
    // Sweep all three windows: ROM bytes match the image exactly; RAM and
    // VRAM hold distinct per-address patterns that stay where written.
    for (uint32_t a = 0; a < kRomSize; ++a)
        EXPECT_EQ(b.bus.read(uint16_t(a)), img[a]);

    for (uint32_t a = 0; a < kRamSize; ++a)
        b.bus.write(uint16_t(kRamBase + a), uint8_t(~a));
    for (uint32_t a = 0; a < kVramSize; ++a)
        b.bus.write(uint16_t(kVramBase + a), uint8_t(a * 3));

    for (uint32_t a = 0; a < kRamSize; ++a)
        EXPECT_EQ(b.bus.read(uint16_t(kRamBase + a)), uint8_t(~a));
    for (uint32_t a = 0; a < kVramSize; ++a)
        EXPECT_EQ(b.bus.read(uint16_t(kVramBase + a)), uint8_t(a * 3));
}

TEST(map, unmapped_reads_float_low_and_writes_drop) {
    Board b;
    EXPECT_EQ(b.bus.read(0x4000), 0x00);          // past VRAM
    EXPECT_EQ(b.bus.read(0xFFFF), 0x00);
    b.bus.write(0x4000, 0xAB);                    // must vanish
    b.bus.write(0xE000, 0xCD);
    EXPECT_EQ(b.bus.read(0x4000), 0x00);
    EXPECT_EQ(b.bus.read(0xE000), 0x00);
}

TEST(map, first_matching_window_wins) {
    Board b;
    RamDevice shadow;
    // Attach a second device over part of RAM AFTER the board windows:
    // the FIRST matching window (the original RAM) keeps winning.
    b.bus.attach(kRamBase, uint16_t(kRamBase + 0xFF), &shadow);
    b.bus.write(0x2000, 0x42);
    EXPECT_EQ(b.ram.read(0x00), 0x42);            // went to the FIRST window
    EXPECT_EQ(shadow.read(0x00), 0x00);           // shadow never sees it
}
