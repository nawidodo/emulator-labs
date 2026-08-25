#define LABSTEST_MAIN
#include "labstest.hpp"
#include "machine.hpp"
#include "frame_io.hpp"

#include <array>
#include <string>

TEST(hash, fnv1a64_known_vectors) {
    // Reference vectors shared with tools/labs/hash_frame.py.
    const uint8_t a = 'a';
    EXPECT_EQ(chip8::fnv1a64_hex(nullptr, 0), "CBF29CE484222325");
    EXPECT_EQ(chip8::fnv1a64_hex(&a, 1), "AF63DC4C8601EC8C");
}

TEST(frame_io, expand_rgba_pixel_convention) {
    chip8::Display d;
    d.set(0, 0, true);
    d.set(63, 31, true);
    std::array<uint8_t, chip8::kRgbaFrameBytes> buf{};
    chip8::expand_rgba(d, buf.data());
    // First pixel white...
    EXPECT_EQ(buf[0], 0xFF);
    EXPECT_EQ(buf[1], 0xFF);
    EXPECT_EQ(buf[2], 0xFF);
    EXPECT_EQ(buf[3], 0xFF);
    // ...second pixel black...
    EXPECT_EQ(buf[4], 0x00);
    // ...last pixel (63,31) white.
    constexpr std::size_t last = chip8::kRgbaFrameBytes - 4;
    EXPECT_EQ(buf[last], 0xFF);
    EXPECT_EQ(buf[last + 3], 0xFF);
}

TEST(frame_io, empty_display_hash_is_stable) {
    chip8::Display d;
    EXPECT_EQ(chip8::frame_hash_hex(d), "B9D103FD6854A325");
}

namespace {
// Draws an 8-wide sprite row at (0,0), then spins: after exactly one frame
// the top-left 8 pixels are lit.
const std::array<uint8_t, 12> kTinyRom = {
    0xA2, 0x0A, 0x60, 0x00, 0x61, 0x00,
    0xD0, 0x11, 0x12, 0x08, 0xFF, 0xFF,
};
}  // namespace

TEST(frame_io, one_frame_of_tiny_rom_hashes_predictably) {
    chip8::Machine m;
    m.load(kTinyRom);
    m.run(chip8::kCyclesPerFrame);
    EXPECT_TRUE(m.display.get(7, 0));
    EXPECT_FALSE(m.display.get(8, 0));
    EXPECT_EQ(chip8::frame_hash_hex(m.display), "39F79FC8E351FD05");
}
