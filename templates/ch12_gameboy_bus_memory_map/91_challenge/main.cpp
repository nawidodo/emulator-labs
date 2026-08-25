// Conformance tests for the CourseBoy-II spec (CHALLENGE.md).
//
// TEST(cb2, ...)   — visible suites covering every rule in the spec.
// TEST(hidden, ...) — grader-only corners students commonly miss; they
//                     are compiled into this binary and filtered by
//                     hidden grading via argv "hidden.".
#define LABSTEST_MAIN
#include <cstdint>
#include <vector>

#include "labstest.hpp"
#include "challenge_map.hpp"

namespace {

using cb2::Cb2Map;

std::vector<uint8_t> makeRom(size_t size = 0x8000) {
    std::vector<uint8_t> rom(size);
    for (size_t i = 0; i < size; ++i)
        rom[i] = static_cast<uint8_t>(i ^ 0x5A);
    return rom;
}

}  // namespace

// ------------------------- visible spec coverage -------------------------

TEST(cb2, fixed_bank_and_mirror_examples) {
    auto rom = makeRom();
    Cb2Map m(rom);
    EXPECT_EQ(m.read(0x0000), rom[0x0000]);
    EXPECT_EQ(m.read(0x3FFF), rom[0x3FFF]);
    EXPECT_EQ(m.read(0x4000), rom[0x0000]);  // mirror start
    EXPECT_EQ(m.read(0x7FFF), rom[0x3FFF]);  // mirror end
}

TEST(cb2, rom_half_ignores_writes) {
    auto rom = makeRom();
    Cb2Map m(rom);
    m.write(0x0123, 0xA5);
    m.write(0x4567, 0x5A);
    EXPECT_EQ(m.read(0x0123), rom[0x0123]);
    EXPECT_EQ(m.read(0x4567), rom[0x0567]);  // mirrored view unchanged too
}

TEST(cb2, vram_window_pages) {
    auto rom = makeRom();
    Cb2Map m(rom);
    EXPECT_EQ(m.vramPage(), 0);              // resets to page 0
    m.write(0x8000 + 0x10, 0x11);            // lands in page 0
    m.write(0xFF00, 0x01);                   // select page 1
    EXPECT_EQ(m.vramPage(), 1);
    EXPECT_EQ(m.read(0x8000 + 0x10), 0x00);  // window now shows page 1...
    m.write(0x8000 + 0x10, 0x22);            // ...which we overwrite here
    EXPECT_EQ(m.peekVram(0x10), 0x11);       // page 0 kept its cell
    EXPECT_EQ(m.peekVram(0x800 + 0x10), 0x22);
    m.write(0xFF00, 0x00);
    EXPECT_EQ(m.read(0x8000 + 0x10), 0x11);  // switch back: original value
}

TEST(cb2, work_ram_alias_roundtrip) {
    auto rom = makeRom();
    Cb2Map m(rom);
    const uint16_t pairs[] = {0xE000, 0xE123, 0xEC80, 0xEFFF};
    for (uint16_t alias : pairs) {
        const uint8_t v = static_cast<uint8_t>(alias >> 4) | 0x01;
        m.write(alias, v);
        EXPECT_EQ(m.read(static_cast<uint16_t>(alias - 0x2000)), v);
        EXPECT_EQ(m.peekRam(alias - 0x2000 - 0xC000), v);
    }
}

TEST(cb2, sys_register_semantics) {
    auto rom = makeRom();
    Cb2Map m(rom);
    m.write(0xFF00, 0xFE);           // only bit 0 decodes
    EXPECT_EQ(m.read(0xFF00), 0x00);
    m.write(0xFF00, 0xFF);
    EXPECT_EQ(m.read(0xFF00), 0x01); // readback is clean 0/1, upper bits zero
    m.write(0xFF01, 0x01);           // dead SYS register: ignored
    m.write(0xFF0F, 0x01);
    EXPECT_EQ(m.read(0xFF00), 0x01); // untouched
    EXPECT_EQ(m.read(0xFF01), 0x00);
    EXPECT_EQ(m.read(0xFF0F), 0x00);
}

TEST(cb2, closed_bands_are_open_bus_ff) {
    auto rom = makeRom();
    Cb2Map m(rom);
    const uint16_t gaps[] = {0x8800, 0x9ABC, 0xBFFF,
                             0xD000, 0xDFFF,
                             0xF000, 0xFEFF,
                             0xFF10, 0xFFFF};
    for (uint16_t a : gaps) {
        m.write(a, 0x77);            // dropped
        EXPECT_EQ(m.read(a), 0xFF);  // strobes tied high on empty slots
    }
}

// ---------------------------- hidden corners -----------------------------

TEST(hidden, mirror_edges_meet_at_the_same_cell) {
    auto rom = makeRom();
    Cb2Map m(rom);
    // 3FFF (fixed side) and 7FFF (mirror side) both resolve to rom[3FFF]:
    EXPECT_EQ(m.read(0x3FFF), rom[0x3FFF]);
    EXPECT_EQ(m.read(0x7FFF), rom[0x3FFF]);
    // 0000 and 4000 both resolve to rom[0000]:
    EXPECT_EQ(m.read(0x4000), rom[0x0000]);
}

TEST(hidden, page_select_masks_everything_but_bit_zero) {
    auto rom = makeRom();
    Cb2Map m(rom);
    const uint8_t attempts[] = {0x02, 0x03, 0xFE, 0xFF};
    for (uint8_t v : attempts) {
        m.write(0xFF00, v);
        EXPECT_EQ(m.vramPage(), v & 0x01);
        EXPECT_EQ(m.read(0xFF00), v & 0x01);  // readback never leaks high bits
    }
}

TEST(hidden, vram_window_never_leaks_page_one_offsets) {
    auto rom = makeRom();
    Cb2Map m(rom);
    m.write(0xFF00, 0x01);               // page 1 visible
    for (uint16_t off = 0; off < 0x800; off += 0x1FF) m.write(0x8000 + off, 0xEE);
    for (uint16_t off = 0; off < 0x800; off += 0x1FF)
        EXPECT_EQ(m.peekVram(off), 0x00);  // page 0 cells unreachable through window
}

TEST(hidden, alias_boundaries_exact_and_fdff_intuition_does_not_carry) {
    auto rom = makeRom();
    Cb2Map m(rom);
    m.write(0xC000, 0x10);
    m.write(0xE000, 0x20);               // same cell as C000
    EXPECT_EQ(m.read(0xC000), 0x20);
    m.write(0xEFFF, 0x30);               // last aliased byte -> CFFF
    EXPECT_EQ(m.peekRam(0xFFF), 0x30);
    m.write(0xCFFF, 0x40);               // direct side of the same cell
    EXPECT_EQ(m.read(0xEFFF), 0x40);
    m.write(0xEFFF, 0x30);
    EXPECT_EQ(m.read(0xF000), 0xFF);     // one past the alias: open bus, NOT a wrap
    EXPECT_EQ(m.read(0xFDFF), 0xFF);     // GB's echo tail does not exist here
}

TEST(hidden, every_closed_band_swept_after_writes) {
    auto rom = makeRom();
    Cb2Map m(rom);
    const struct { uint16_t lo, hi; } bands[] = {
        {0x8800, 0xBFFF}, {0xD000, 0xDFFF}, {0xF000, 0xFEFF}, {0xFF10, 0xFFFF},
    };
    for (const auto& b : bands) {
        for (uint32_t a = b.lo; a <= b.hi; a += 0x137)
            m.write(static_cast<uint16_t>(a), 0x42);
        for (uint32_t a = b.lo; a <= b.hi; a += 0x259)
            EXPECT_EQ(m.read(static_cast<uint16_t>(a)), 0xFF);
    }
    // The writes must not have leaked into real RAM either:
    for (size_t i = 0; i < 0x1000; i += 0xDD) EXPECT_EQ(m.peekRam(i), 0x00);
}

TEST(hidden, short_image_pads_with_open_bus) {
    auto rom = makeRom(0x2000);  // half-size image
    Cb2Map m(rom);
    EXPECT_EQ(m.read(0x1FFF), rom[0x1FFF]);  // last real byte
    EXPECT_EQ(m.read(0x2000), 0xFF);         // past the end: $FF, not garbage
    EXPECT_EQ(m.read(0x7FFF), 0xFF);
}
