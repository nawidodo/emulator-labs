#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>

#include "chip8.hpp"

using chip8::Chip8;

namespace {

// Exact bytes of tests/public/ch03_chip8_architecture/roms/ibm_logo.ch8
// (see roms/ibm_logo.asm.txt next to it for the annotated listing).
constexpr uint8_t kLogoRom[] = {
    0x00, 0xE0, 0x60, 0x06, 0x61, 0x0C, 0xA2, 0x1E, 0xD0, 0x18, 0x70,
    0x0C, 0xA2, 0x26, 0xD0, 0x18, 0x70, 0x0C, 0xA2, 0x2E, 0xD0, 0x18,
    0x70, 0x0C, 0xA2, 0x36, 0xD0, 0x18, 0x12, 0x1C,
    // Glyph data: 'C', 'H', 'I', 'P', 8 rows each.
    0x3C, 0x66, 0xC3, 0xC0, 0xC0, 0xC3, 0x66, 0x3C,
    0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3,
    0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C,
    0xFE, 0xFF, 0xC3, 0xC3, 0xFF, 0xFE, 0xC0, 0xC0,
};

uint64_t fnv1a64(const uint8_t* data, size_t size) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t n = 0; n < size; ++n) {
        h ^= data[n];
        h *= 0x100000001B3ULL;
    }
    return h;
}

}  // namespace

TEST(drw, draws_sprite_bits) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x60, 0x00,             // V0 = 0 (x)
                           0x61, 0x00,             // V1 = 0 (y)
                           0xA2, 0x08,             // I = 0x208
                           0xD0, 0x11,             // DRW V0,V1,1
                           0xF0, 0x0F};            // sprite row at 0x208
    c.load(rom);
    for (int n = 0; n < 4; ++n) c.step();
    EXPECT_TRUE(c.pixel(0, 0));
    EXPECT_TRUE(c.pixel(1, 0));
    EXPECT_FALSE(c.pixel(4, 0));
}

TEST(drw, collision_sets_vf_and_erases) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x60, 0x00, 0x61, 0x00, 0xA2, 0x08, 0xD0, 0x11,
                           0xF0, 0x0F, 0xD0, 0x11};  // draw twice at 0,0
    c.load(rom);
    for (int n = 0; n < 6; ++n) c.step();
    EXPECT_EQ(c.v(0xF), 1);      // second pass erased lit pixels
    EXPECT_FALSE(c.pixel(0, 0)); // XOR back to black
}

TEST(drw, clips_at_screen_edge_without_wrapping) {
    Chip8 c;
    c.reset();
    const uint8_t rom[] = {0x60, 63,   // V0 = 63: only column 63 fits
                           0x61, 31,   // V1 = 31: only row 31 fits
                           0xA2, 0x08,
                           0xD0, 0x12,          // DRW V0,V1 with N=2 rows
                           0xFF, 0x81};
    c.load(rom);
    for (int n = 0; n < 4; ++n) c.step();
    EXPECT_TRUE(c.pixel(63, 31));  // first bit of first row survives
    EXPECT_EQ(c.v(0xF), 0);        // nothing LIT was erased by clipping
}

// The challenge itself: after the 15 instructions of the program (it then
// parks in a self-jump), the word CHIP is rendered at y=12 starting x=6 and
// the framebuffer hashes to the committed golden value.
TEST(challenge, ibm_style_logo_frame_is_stable) {
    Chip8 c;
    c.reset();
    c.load(kLogoRom);
    for (int n = 0; n < 20; ++n) c.step();
    EXPECT_EQ(c.pc(), 0x21C);  // parked in the final self-jump
    EXPECT_EQ(c.v(0xF), 0);    // disjoint glyphs -> no collisions

    // Spot-check the glyphs.
    EXPECT_TRUE(c.pixel(8, 12));    // top arc of 'C'
    EXPECT_TRUE(c.pixel(22, 15));   // crossbar of 'H'
    EXPECT_TRUE(c.pixel(33, 16));   // stem of 'I'
    EXPECT_TRUE(c.pixel(43, 12));   // bowl of 'P'
    EXPECT_FALSE(c.pixel(15, 12));  // gap between C and H

    const auto& px = c.pixels();
    const uint64_t hash = fnv1a64(px.data(), px.size());
    // Golden FNV-1a 64 over the 64x32 framebuffer after this exact program;
    // see tests/public/ch03_chip8_architecture/provenance.md.
    EXPECT_EQ(hash, 0x84AFB2AD021998CDULL);
}
