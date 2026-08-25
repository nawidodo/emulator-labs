// Tests for 99_coding_test: Tetra-8 spec examples (CODING_TEST.md).
// TEST(t8, ...) is the visible set; TEST(hidden, ...) carries grader
// corners filtered by hidden grading via argv "hidden.".
#define LABSTEST_MAIN
#include <cstdint>
#include <vector>

#include "labstest.hpp"
#include "coding_map.hpp"

namespace {

using t8::Tetra8Map;

// Bank k (each $2000 slice) is filled with byte k+1 so any misbank shows.
std::vector<uint8_t> makeFourBankRom() {
    std::vector<uint8_t> rom(0x10000);
    for (size_t i = 0; i < rom.size(); ++i)
        rom[i] = static_cast<uint8_t>(i / 0x2000 + 1);
    return rom;
}

}  // namespace

TEST(t8, bank_register_semantics) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    EXPECT_EQ(m.bank(), 1);        // reset value
    m.write(0xFF00, 3);
    EXPECT_EQ(m.bank(), 3);
    m.write(0xFF00, 7);            // masked to two bits: still 3
    EXPECT_EQ(m.bank(), 3);
    m.write(0xFF00, 4);            // 4&3 == 0 -> wraps to 1
    EXPECT_EQ(m.bank(), 1);
    m.write(0xFF00, 0);
    EXPECT_EQ(m.bank(), 1);
    EXPECT_EQ(m.read(0xFF00), 1);  // readback
}

TEST(t8, window_reads_follow_the_selected_bank) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    EXPECT_EQ(m.read(0x0100), rom[0x0100]);   // fixed half always bank 0 area
    EXPECT_EQ(m.read(0x4000), 2);             // reset bank 1 -> slice 1 marker
    EXPECT_EQ(m.read(0x5FFF), 2);
    m.write(0xFF00, 2);
    EXPECT_EQ(m.read(0x4000), 3);
    m.write(0xFF00, 3);
    EXPECT_EQ(m.read(0x5FFF), 4);
}

TEST(t8, nul_shadow_past_image_end) {
    auto rom = makeFourBankRom( );
    rom.resize(0x6000);                       // banks 0-2 only
    Tetra8Map m(rom);
    m.write(0xFF00, 3);                       // selects a bank past the image
    EXPECT_EQ(m.read(0x4000), 0x00);          // NUL shadow, not garbage/$FF
    EXPECT_EQ(m.read(0x5FFF), 0x00);
    m.write(0xFF00, 2);
    EXPECT_EQ(m.read(0x4000), 3);             // in-image bank still real
    rom.resize(0x1000);                       // tiny image: fixed side clipped too
    Tetra8Map small(rom);
    EXPECT_EQ(small.read(0x3FFF), 0x00);
}

TEST(t8, ram_shadow_roundtrip_at_both_edges) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    const uint16_t pairs[] = {0xD000, 0xD123, 0xD5FF};  // D5FF is the last shadow byte
    for (uint16_t sh : pairs) {
        const uint8_t v = static_cast<uint8_t>(sh >> 4) | 0x01;
        const uint16_t ramAddr = static_cast<uint16_t>(sh - 0x3000);
        m.write(sh, v);                            // via shadow
        EXPECT_EQ(m.peekRam(ramAddr - 0xA000), v); // landed in the RAM cells
        EXPECT_EQ(m.read(ramAddr), v);             // via RAM side
        m.write(ramAddr, static_cast<uint8_t>(~v));
        EXPECT_EQ(m.read(sh), static_cast<uint8_t>(~v));
    }
    // One past the shadow line: closed slot, reads NUL, drops writes.
    m.write(0xD600, 0xEE);
    EXPECT_EQ(m.read(0xD600), 0x00);
    EXPECT_EQ(m.read(0xA600), 0x00);
}

TEST(t8, scratchpad_mirror_is_the_same_cells) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    m.write(0xE000, 0x10);
    m.write(0xE001, 0x20);
    EXPECT_EQ(m.read(0xE800), 0x10);          // mirror start
    EXPECT_EQ(m.read(0xE801), 0x20);
    m.write(0xEFFF, 0x30);                    // mirror end -> E7FF cell
    EXPECT_EQ(m.read(0xE7FF), 0x30);
    EXPECT_EQ(m.peekScratch(0x7FF), 0x30);
    EXPECT_EQ(m.read(0xF000), 0x00);          // one past: closed slot again
}

TEST(t8, closed_slots_read_nul_and_drop_everywhere) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    const uint16_t gaps[] = {0x6000, 0x7FFF, 0x9ABC, 0xB000,
                             0xCDEF, 0xF123, 0xFF01, 0xFFFF};
    for (uint16_t a : gaps) {
        m.write(a, 0x77);
        EXPECT_EQ(m.read(a), 0x00);           // low-idling bus, never $FF
    }
    // Closed-slot writes must not leak into live RAM either:
    for (size_t i = 0; i < 0x1000; i += 0x101) EXPECT_EQ(m.peekRam(i), 0x00);
}

TEST(t8, ram_powers_on_nul_filled) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    for (uint16_t a = 0xA000; a <= 0xAFFF; a += 0x213)
        EXPECT_EQ(m.read(a), 0x00);
}

// ---------------------------- hidden corners -----------------------------

TEST(hidden, bank_mask_and_wrap_corners) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    const uint8_t cases[][2] = {
        {0xFC, 1}, {0xFD, 1}, {0xFE, 2}, {0xFF, 3},
        {0x04, 1}, {0x83, 3}, {0x40, 1},
    };
    for (const auto& [val, want] : cases) {
        m.write(0xFF00, val);
        EXPECT_EQ(m.bank(), want);
    }
}

TEST(hidden, first_windowed_read_uses_reset_bank_before_any_write) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    EXPECT_EQ(m.read(0x4567), 2);  // bank 1 without ever writing FF00
}

TEST(hidden, nul_shadow_boundary_inside_one_bank) {
    auto rom = makeFourBankRom();
    rom.resize(0x5000);            // bank 2 partial: last full offset 0xFFF
    Tetra8Map m(rom);
    m.write(0xFF00, 2);
    EXPECT_EQ(m.read(0x4000 + 0x0FFF), rom[0x4FFF]);  // last real byte
    EXPECT_EQ(m.read(0x4000 + 0x1000), 0x00);         // one past: NUL shadow
}

TEST(hidden, shadow_and_scratch_never_overlap_closed_slots) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    // Write through every alias path, then prove closed neighbours are dead:
    m.write(0xD5FF, 0x11);  // last shadow byte -> A5FF
    EXPECT_EQ(m.peekRam(0x5FF), 0x11);
    m.write(0xA300, 0x22);  // direct RAM just past the shadowed band
    EXPECT_EQ(m.read(0xD600), 0x00);  // has NO shadow line
    m.write(0xE7FF, 0x33);
    m.write(0xE800, 0x44);  // mirror of E000
    EXPECT_EQ(m.peekScratch(0x7FF), 0x33);
    EXPECT_EQ(m.peekScratch(0x000), 0x44);
}

TEST(hidden, rom_half_drops_writes_even_over_the_window) {
    auto rom = makeFourBankRom();
    Tetra8Map m(rom);
    m.write(0x0100, 0xAA);
    m.write(0x4500, 0xBB);
    EXPECT_EQ(m.read(0x0100), rom[0x0100]);
    EXPECT_EQ(m.read(0x4500), rom[0x2500]);  // unchanged view through bank 1
}
