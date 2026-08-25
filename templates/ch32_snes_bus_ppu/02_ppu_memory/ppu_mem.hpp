#pragma once
// Exercise 02 — PPU memory: VRAM, CGRAM and OAM.
//
// VRAM is 64 KiB but is addressed as 32 K WORDS (a word is the 16-bit color
// unit tiles are made of). CGRAM holds 256 BGR555 entries. OAM stores 512
// sprites split across two tables: a 512-sprite x 4-byte low table (2048
// bytes) plus a 128-byte high table carrying two extra bits per sprite.
// (Real hardware has 128 sprites in 544 bytes; this chapter models the
// 512-sprite layout specified by the curriculum.)
//
// Field layouts used throughout this chapter (documented simplifications of
// the real register semantics; bit positions match hardware):
//
//   BGR555:   bits 0-4 red, 5-9 green, 10-14 blue
//   Map entry (16-bit): bits 0-9 tile, 10-12 palette,
//                       13 priority, 14 hflip, 15 vflip
//   OAM low table, 4 bytes per sprite i at low[i*4 .. i*4+3]:
//       +0  x low 8 bits
//       +1  y (8 bits)
//       +2  tile number bits 0-7
//       +3  bits 0-1 tile bits 8-9, bits 2-4 palette (3 bits),
//           bit 5 hflip, bit 6 vflip, bit 7 priority
//   OAM high table byte high[i/4], sprite's bits at (i%4)*2:
//       bit 0  x msb (bit 8), bit 1  size-select msb

#include <array>
#include <cstdint>

namespace snesbus {

//@LABS-BEGIN 1
//@LABS-SOLUTION
struct Vram {
    std::array<uint16_t, 32768> w{};  // 32 K words = 64 KiB

    // Word-addressed; the address wraps modulo 32 K like the PPU's counter.
    uint16_t read(uint16_t addr) const { return w[addr & 0x7FFFu]; }
    void write(uint16_t addr, uint16_t v) { w[addr & 0x7FFFu] = v; }

    // Byte view: even addresses are the LOW byte of their word, odd
    // addresses the HIGH byte. This is how $2118/$2119 streaming sees VRAM.
    uint8_t read_byte(uint16_t addr) const {
        const uint16_t v = read(static_cast<uint16_t>(addr >> 1));
        return (addr & 1u) ? static_cast<uint8_t>(v >> 8)
                           : static_cast<uint8_t>(v);
    }
};
//@LABS-STUB
struct Vram {
    std::array<uint16_t, 32768> w{};  // 32 K words = 64 KiB

    // TODO(1): implement word read/write wrapping on 15 bits, plus the
    // little-endian byte view (even addr = low byte).
    uint16_t read(uint16_t) const {
        return 0;  // wrong on purpose
    }
    void write(uint16_t, uint16_t) {
        // TODO(1): replace this body.
    }
    uint8_t read_byte(uint16_t) const {
        return 0;  // wrong on purpose
    }
};
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Expand a BGR555 color to 0x00RRGGBB. Each 5-bit channel is replicated
// into 8 bits with ((c << 3) | (c >> 2)) so that $1F maps to 0xFF exactly.
inline uint32_t bgr555_to_rgb888(uint16_t c) {
    const unsigned r5 = c & 0x1Fu;
    const unsigned g5 = (c >> 5) & 0x1Fu;
    const unsigned b5 = (c >> 10) & 0x1Fu;
    const auto expand = [](unsigned v) {
        return static_cast<uint8_t>((v << 3) | (v >> 2));
    };
    return static_cast<uint32_t>(expand(r5) << 16) |
           static_cast<uint32_t>(expand(g5) << 8) |
           static_cast<uint32_t>(expand(b5));
}
//@LABS-STUB
// TODO(2): expand BGR555 to 0x00RRGGBB using 5->8 bit replication
// ((c << 3) | (c >> 2) per channel). Red lives in bits 0-4.
inline uint32_t bgr555_to_rgb888(uint16_t) {
    return 0;  // wrong on purpose: every color renders black
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
struct Cgram {
    std::array<uint16_t, 256> entry{};

    // The CPU writes CGRAM one byte at a time through $2122; the byte
    // address is the 9-bit little-endian byte offset (0..511): even = low
    // half of entry addr>>1, odd = high half.
    void write_byte(uint16_t addr, uint8_t v) {
        if (addr & 1u) {
            entry[addr >> 1] =
                static_cast<uint16_t>((entry[addr >> 1] & 0x00FFu) |
                                      static_cast<uint16_t>(v) << 8);
        } else {
            entry[addr >> 1] =
                static_cast<uint16_t>((entry[addr >> 1] & 0xFF00u) | v);
        }
    }
    uint8_t read_byte(uint16_t addr) const {
        const uint16_t v = entry[addr >> 1];
        return (addr & 1u) ? static_cast<uint8_t>(v >> 8)
                           : static_cast<uint8_t>(v);
    }
};
//@LABS-STUB
struct Cgram {
    std::array<uint16_t, 256> entry{};

    // TODO(3): interleave byte access — even addresses write/read the low
    // byte of entry[addr>>1], odd addresses the high byte.
    void write_byte(uint16_t, uint8_t) {
        // TODO(3): replace this body.
    }
    uint8_t read_byte(uint16_t) const {
        return 0;  // wrong on purpose
    }
};
//@LABS-END

struct Sprite {
    uint16_t x = 0;        // 9 bits (low table byte 0 + high-table msb)
    uint8_t y = 0;
    uint16_t tile = 0;     // 10 bits
    uint8_t palette = 0;   // 3 bits
    bool priority = false;
    bool hflip = false;
    bool vflip = false;
    bool size = false;     // size-select msb from the high table
};

//@LABS-BEGIN 4
//@LABS-SOLUTION
struct Oam {
    std::array<uint8_t, 2048> low{};  // 512 sprites x 4 bytes
    std::array<uint8_t, 128> high{};  // 2 bits per sprite

    Sprite sprite(unsigned idx) const {
        idx &= 511u;
        const unsigned b = static_cast<unsigned>(idx) * 4u;
        const unsigned attr = low[b + 3];
        const uint8_t h = high[idx >> 2];
        const unsigned pair = (h >> ((idx & 3u) * 2u)) & 3u;
        Sprite s;
        s.x = static_cast<uint16_t>(low[b] |
                                    ((pair & 1u) ? 0x100u : 0u));
        s.y = low[b + 1];
        s.tile = static_cast<uint16_t>(low[b + 2] | ((attr & 3u) << 8));
        s.palette = static_cast<uint8_t>((attr >> 2) & 7u);
        s.priority = (attr & 0x80u) != 0;
        s.hflip = (attr & 0x20u) != 0;
        s.vflip = (attr & 0x40u) != 0;
        s.size = (pair & 2u) != 0;
        return s;
    }
};
//@LABS-STUB
struct Oam {
    std::array<uint8_t, 2048> low{};  // 512 sprites x 4 bytes
    std::array<uint8_t, 128> high{};  // 2 bits per sprite

    // TODO(4): combine both tables into a Sprite. See the field-layout table
    // in the header comment; the high table carries the x msb (bit 0) and
    // size msb (bit 1) for each sprite.
    Sprite sprite(unsigned) const {
        return Sprite{};  // wrong on purpose: always an idle sprite at 0,0
    }
};
//@LABS-END



}  // namespace snesbus
