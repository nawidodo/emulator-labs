#define LABSTEST_MAIN
#include "labstest.hpp"

#include "ppu_mem.hpp"

using snesbus::bgr555_to_rgb888;
using snesbus::Cgram;
using snesbus::Oam;
using snesbus::Sprite;
using snesbus::Vram;

TEST(vram, word_round_trip_and_wrap) {
    Vram v;
    v.write(0, 0xBEEF);
    v.write(0x7FFF, 0x1234);
    EXPECT_EQ(v.read(0), 0xBEEF);
    EXPECT_EQ(v.read(0x7FFF), 0x1234);
    // Address wraps modulo 32 K words.
    v.write(0x8000, 0xCAFE);
    EXPECT_EQ(v.read(0), 0xCAFE);
    v.write(0xFFFF, 0x5678);
    EXPECT_EQ(v.read(0x7FFF), 0x5678);
}

TEST(vram, byte_view_interleave_round_trip) {
    Vram v;
    // Streaming bytes through the $2118/$2119-style view must reconstruct
    // little-endian words.
    for (unsigned i = 0; i < 64; ++i) {
        const uint16_t word = static_cast<uint16_t>(i * 257 + 0x0102);
        const uint16_t addr = static_cast<uint16_t>(i);
        v.write(addr, word);
        EXPECT_EQ(v.read_byte(static_cast<uint16_t>(addr * 2)),
                  static_cast<uint8_t>(word));
        EXPECT_EQ(v.read_byte(static_cast<uint16_t>(addr * 2 + 1)),
                  static_cast<uint8_t>(word >> 8));
    }
}

// Conversion vectors: exact BGR555 -> RGB888 expansion.
TEST(cgram_color, expansion_vectors) {
    EXPECT_EQ(bgr555_to_rgb888(0x0000), 0x000000u);  // black
    EXPECT_EQ(bgr555_to_rgb888(0x7FFF), 0xFFFFFFu);  // white: 31 -> 255
    EXPECT_EQ(bgr555_to_rgb888(0x001F), 0xFF0000u);  // full red
    EXPECT_EQ(bgr555_to_rgb888(0x03E0), 0x00FF00u);  // full green
    EXPECT_EQ(bgr555_to_rgb888(0x7C00), 0x0000FFu);  // full blue

    // Mid-scale replication: channel value 15 -> (15<<3)|(15>>2) = 123.
    EXPECT_EQ(bgr555_to_rgb888(0x000F), (123u << 16));
    EXPECT_EQ(bgr555_to_rgb888(0x01E0), (123u << 8));
    EXPECT_EQ(bgr555_to_rgb888(0x3C00), 123u);

    // Channel value 1 -> (1<<3)|(1>>2) = 8.
    EXPECT_EQ(bgr555_to_rgb888(0x0001), 0x080000u);
}

TEST(cgram, byte_interleave_round_trip) {
    Cgram c;
    for (unsigned i = 0; i < 256; ++i) {
        c.write_byte(static_cast<uint16_t>(i * 2u),
                     static_cast<uint8_t>(i + 1));
        c.write_byte(static_cast<uint16_t>(i * 2u + 1u),
                     static_cast<uint8_t>(i + 200));
    }
    for (unsigned i = 0; i < 256; ++i) {
        const uint16_t expect =
            static_cast<uint16_t>((i + 1) | ((i + 200) << 8));
        EXPECT_EQ(c.entry[i], expect);
        EXPECT_EQ(c.read_byte(static_cast<uint16_t>(i * 2u)),
                  static_cast<uint8_t>(i + 1));
        EXPECT_EQ(c.read_byte(static_cast<uint16_t>(i * 2u + 1u)),
                  static_cast<uint8_t>(i + 200));
    }
}

namespace {

// Assemble one sprite's OAM image from explicit fields.
void poke(Oam& oam, unsigned idx, uint16_t x, uint8_t y, uint16_t tile,
          uint8_t pal, bool prio, bool hf, bool vf, bool size) {
    const unsigned b = idx * 4u;
    oam.low[b] = static_cast<uint8_t>(x & 0xFFu);
    oam.low[b + 1] = y;
    oam.low[b + 2] = static_cast<uint8_t>(tile & 0xFFu);
    unsigned attr = (tile >> 8) & 3u;
    attr |= static_cast<unsigned>(pal & 7u) << 2;
    if (hf) attr |= 0x20u;
    if (vf) attr |= 0x40u;
    if (prio) attr |= 0x80u;
    oam.low[b + 3] = static_cast<uint8_t>(attr);
    uint8_t& h = oam.high[idx >> 2];
    const unsigned shift = (idx & 3u) * 2u;
    h = static_cast<uint8_t>(
        (h & ~(3u << shift)) |
        ((size ? 2u : 0u) | ((x >> 8) & 1u ? 1u : 0u)) << shift);
}

}  // namespace

TEST(oam, field_decode_from_both_tables) {
    Oam oam;
    poke(oam, 0, 0x0102, 0x34, 0x02AB, 5, true, false, true, true);
    const Sprite s0 = oam.sprite(0);
    EXPECT_EQ(s0.x, 0x102);
    EXPECT_EQ(s0.y, 0x34);
    EXPECT_EQ(s0.tile, 0x2AB);
    EXPECT_EQ(s0.palette, 5);
    EXPECT_TRUE(s0.priority);
    EXPECT_FALSE(s0.hflip);
    EXPECT_TRUE(s0.vflip);
    EXPECT_TRUE(s0.size);

    // Sprite 33 lives in a different high-table byte and bit pair.
    poke(oam, 33, 0x0011, 0xF0, 0x0007, 2, false, true, false, false);
    const Sprite s33 = oam.sprite(33);
    EXPECT_EQ(s33.x, 0x11);
    EXPECT_EQ(s33.tile, 0x007);
    EXPECT_EQ(s33.palette, 2);
    EXPECT_FALSE(s33.priority);
    EXPECT_TRUE(s33.hflip);
    EXPECT_FALSE(s33.size);

    // Writing sprite 33 must not disturb sprite 0's high-table bits.
    EXPECT_EQ(oam.sprite(0).x, 0x102);
    EXPECT_TRUE(oam.sprite(0).size);
}

TEST(oam, all_512_sprites_addressable) {
    Oam oam;
    for (unsigned i = 0; i < 512; ++i) {
        poke(oam, i, static_cast<uint16_t>(i), static_cast<uint8_t>(i),
             static_cast<uint16_t>(i & 0x3FF), static_cast<uint8_t>(i & 7),
             false, false, false, false);
    }
    for (unsigned i = 0; i < 512; ++i) {
        const Sprite s = oam.sprite(i);
        EXPECT_EQ(s.x, i);          // x msb set once i exceeds 255
        EXPECT_EQ(s.y, i & 0xFF);
        EXPECT_EQ(s.tile, i & 0x3FF);
        EXPECT_EQ(s.palette, i & 7);
    }
    EXPECT_EQ(oam.sprite(256).x, 256);  // needs the high-table msb
}
